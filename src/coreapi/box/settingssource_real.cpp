/*
 * settingssource_real.cpp - settings read from and written to the live configuration
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "coreapi/base/apply.h"
#include "coreapi/base/deps.h"
#include "coreapi/base/flagfile.h"
#include "coreapi/base/schema.h"
#include "coreapi/box/applyworker.h"
#include "coreapi/settings/settingstable.h"

#include <neutrinoMessages.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <pthread.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <neutrinoMessages.h>

namespace coreapi
{

namespace
{

/* Reads the values the program is running on, not the file it saves them to.
   Between a load and a save the file holds what was last written and the struct
   holds what is in effect, so a read of the file answers a setting the box has
   already changed, and a write to it is overwritten by the next save.

   A write is held here and carried to the struct by the program's own loop,
   because a caller reaches this from another thread while the loop is using
   those values and several of them are strings. Saving is asked of that loop in
   the same message, so the two cannot be split: a reload arriving between them
   would throw the write away. */
class RealSettingsSource : public SettingsSource
{
	public:
		RealSettingsSource() : values(0), save(0), stamp(0) {}

		void bind(SNeutrinoSettings *v, bool (*s)())
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			held.clear();
			values = v;
			save = s;
		}

		Status readInt(const char *key, long &out) const
		{
			const Descriptor *d = find(key);
			if (d == 0)
				return status(d);

			/* A flag that is a file's existence. What was written here answers first, as for any
			   row, and otherwise the file is looked at: the path is the row's name. */
			if (d->field.origin == FieldOrigin::FlagFile)
			{
				{
					OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
					const Write *w = latest(d);
					if (w)
					{
						out = w->number;
						return Status::Ok;
					}
				}
				out = flagFileIsSet(d->field.name) ? 1 : 0;
				return Status::Ok;
			}

			/* A value a daemon holds rather than a field. What was written here answers first, and
			   the daemon is asked with the guard let go: reaching it is a blocking exchange over a
			   socket. A daemon that cannot be reached is an answer and not a nought: nought is a real
			   value, so answering it would report a recording as starting when the programme does. */
			if (d->field.ask != 0)
			{
				{
					OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
					const Write *w = latest(d);
					if (w)
					{
						out = w->number;
						return Status::Ok;
					}
				}
				return d->field.ask(out) ? Status::Ok : Status::NotSupported;
			}

			if (d->field.read_number == 0)
				return Status::NotSupported;

			// What was written here reads back before the loop has taken it, or
			// a caller that writes and reads is told its own write did nothing.
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			const Write *w = latest(d);
			out = w ? w->number : d->field.read_number(*values);
			return Status::Ok;
		}

		Status readString(const char *key, std::string &out) const
		{
			const Descriptor *d = find(key);
			if (d == 0)
				return status(d);
			if (d->field.read_text == 0)
				return Status::NotSupported;

			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			const Write *w = latest(d);
			if (w)
				out = w->text;
			else
				d->field.read_text(*values, out);
			return Status::Ok;
		}

		Status writeInt(const char *key, long value)
		{
			const Descriptor *d = find(key);
			if (d == 0)
				return status(d);
			if (d->field.write_number == 0 && d->field.tell == 0 &&
			    d->field.origin != FieldOrigin::FlagFile)
				return Status::NotSupported;
			/* The fields are narrower than a long, so a value that does not fit is refused now
			   rather than stored as a different one later: what stores it runs on another thread and
			   has nobody left to answer. A value a daemon holds has no field to fit it into, so the
			   row's own bounds are the whole rule. */
			if (d->field.fits_number != 0 && !d->field.fits_number(value))
				return Status::InvalidArgument;

			Write w;
			w.row = d;
			w.number = value;
			w.owner = caller();
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			held.push_back(w);
			return Status::Ok;
		}

		Status writeString(const char *key, const std::string &value)
		{
			const Descriptor *d = find(key);
			if (d == 0)
				return status(d);
			if (d->field.write_text == 0)
				return Status::NotSupported;

			Write w;
			w.row = d;
			w.text = value;
			w.owner = caller();
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			held.push_back(w);
			return Status::Ok;
		}

		Status readList(const char *key, std::vector<std::string> &out) const
		{
			const Descriptor *d = find(key);
			if (d == 0)
				return status(d);
			const FieldExtra *x = d->field.extra;
			if (x == 0 || x->read_list == 0)
				return Status::NotSupported;

			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			const Write *w = latest(d);
			if (w)
				out = w->list;
			else
				x->read_list(*values, out);
			return Status::Ok;
		}

		Status writeList(const char *key, const std::vector<std::string> &value)
		{
			const Descriptor *d = find(key);
			if (d == 0)
				return status(d);
			const FieldExtra *x = d->field.extra;
			if (x == 0 || x->write_list == 0)
				return Status::NotSupported;

			Write w;
			w.row = d;
			w.list = value;
			w.owner = caller();
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			held.push_back(w);
			return Status::Ok;
		}

		Status readRecords(const char *key, std::vector<RecordValues> &out) const
		{
			const Descriptor *d = find(key);
			if (d == 0)
				return status(d);
			const FieldExtra *x = d->field.extra;
			if (x == 0 || x->read_records == 0)
				return Status::NotSupported;

			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			const Write *w = latest(d);
			if (w)
				out = w->records;
			else
				x->read_records(*values, out);
			return Status::Ok;
		}

		Status writeRecords(const char *key, const std::vector<RecordValues> &value)
		{
			const Descriptor *d = find(key);
			if (d == 0)
				return status(d);
			const FieldExtra *x = d->field.extra;
			if (x == 0 || x->write_records == 0)
				return Status::NotSupported;

			Write w;
			w.row = d;
			w.records = value;
			w.owner = caller();
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			held.push_back(w);
			return Status::Ok;
		}

		Status persistNow()
		{
			if (!values)
				return Status::Internal;
			if (save == 0)
				return Status::NotSupported;
			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
				stampMine(caller());
			}
			applyAndSave();
			return Status::Ok;
		}

		/* Asks the loop to take what was written and save it. What comes back says whether the
		   loop took the message, not whether the file was written: the loop answers nothing, and
		   waiting for it from here is what deadlocks.

		   What this call promises is what this caller wrote and no more. A call that stamped
		   everything held would take a second caller's write with it and, on a refused message,
		   throw that write away while answering its own caller ok. */
		Status persist()
		{
			if (!values)
				return Status::Internal;
			if (save == 0)
				return Status::NotSupported;

			unsigned batch = 0;
			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
				batch = stampMine(caller());
			}

			/* Posted with the lock let go, because the loop takes the same lock
			   to drain what was posted: waiting for room in its queue while
			   holding it is what deadlocks. */
			Result<void> posted = postCommand(NeutrinoMessages::APPLY_SETTINGS, 0);
			if (posted.ok())
				return Status::Ok;

			/* Nothing will carry this batch now, so it is taken back rather than left for the next
			   message to sweep up: a caller told its value was not written must not read it back,
			   and a write of some other setting must not be what puts it into the box. */
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			std::vector<Write> kept;
			kept.reserve(held.size());
			for (size_t i = 0; i < held.size(); ++i)
			{
				if (held[i].batch != batch)
					kept.push_back(held[i]);
			}
			held.swap(kept);
			return posted.error().status;
		}

		/* On the loop's own thread: everything written since the last time, then the program's
		   save, then whoever applies each of them. In that order, or the save writes the values it
		   had before and the notifiers read the value the box was running on. */
		void applyAndSave()
		{
			/* The registry below is unlocked and belongs to the program's loop, so a drain
			   from anywhere else is refused whole: what was written stays held for the loop
			   to take. */
			if (!onApplyLoop())
			{
				std::fprintf(stderr, "coreapi: written settings were drained on a thread that is not the loop and were left held\n");
				return;
			}
			std::vector<Write> taken;
			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
				if (!values)
					return;
				/* Only what a post promised. A write nobody has posted yet is a batch its
				   caller is still filling, and a drain that took it would apply that batch
				   in halves. What a refused post would have carried is not here either: that
				   call took its own back before answering. */
				std::vector<Write> kept;
				for (size_t i = 0; i < held.size(); ++i)
				{
					if (held[i].batch != 0)
						taken.push_back(held[i]);
					else
						kept.push_back(held[i]);
				}
				held.swap(kept);

				/* Under the same lock the reads take, or a read of a value runs
				   beside the write of it. This one holds the list of pending
				   writes; the struct's own text lock is taken inside each field
				   function, which is what the screens on the box's loop take
				   too. Only ever in that order: nothing holds the text lock and
				   then asks for this one. */
				for (size_t i = 0; i < taken.size(); ++i)
				{
					const FieldRef &f = taken[i].row->field;
					if (f.write_number)
						f.write_number(*values, taken[i].number);
					else if (f.write_text)
						f.write_text(*values, taken[i].text);
					else if (f.extra && f.extra->write_list)
						f.extra->write_list(*values, taken[i].list);
					else if (f.extra && f.extra->write_records)
						f.extra->write_records(*values, taken[i].records);
				}
			}

			/* A flag file is made or removed here, on the loop, so that the file and the
			   program's own settings change together and a read in between never sees one
			   without the other. Nothing of it is in the struct or the settings file. */
			for (size_t i = 0; i < taken.size(); ++i)
			{
				const FieldRef &f = taken[i].row->field;
				if (f.origin != FieldOrigin::FlagFile)
					continue;
				if (!setFlagFile(f.name, taken[i].number != 0))
					std::fprintf(stderr, "coreapi: %s was written and the file could not be changed\n",
						     taken[i].row->key);
			}

			/* The daemons are told with the guard let go, each being a blocking exchange over a
			   socket. Before the save, so what a caller sees is the order the members take. The save
			   carries none of these, none of them ever having been in the file. */
			for (size_t i = 0; i < taken.size(); ++i)
			{
				const FieldRef &f = taken[i].row->field;
				// Reported here because it reaches no caller: the write was
				// answered before this runs.
				if (f.tell && !f.tell(taken[i].number))
					std::fprintf(stderr, "coreapi: %s was written and the daemon holding it could not be told\n",
						     taken[i].row->key);
			}

			if (save)
				save();

			/* Driven by the writes themselves rather than by a list of keys kept beside them: a
			   second record of the same thing can come apart from the first, and a value that landed
			   without its group is the defect this exists to stop. A setting only a restart applies
			   has nobody to tell, and one read where it is used needs nothing.

			   A key a group holds goes to that group, once per drain however many of its keys were
			   written. A key with no group has no effect to run, which the apply scan holds every
			   row to. The registry has no lock and belongs to this thread, the one the program's
			   loop drains on, so nothing here may be moved to the thread that wrote. A group whose
			   phase of startup is not reached yet is skipped and run by that phase, with the values
			   it finds then, so a write that arrives early is not lost. */
			std::vector<std::string> grouped;
			for (size_t i = 0; i < taken.size(); ++i)
			{
				const Descriptor *row = taken[i].row;
				if (row->needs_restart)
					continue;
				/* A group reads its values through the rows, so it runs for a row of any origin: a
				   colour, a flag file, a daemon's value and a bit of a mask are all written by this
				   layer, and a group left out for them is a write that lands and is never applied. */
				if (groupOf(row->key) != NULL)
					grouped.push_back(row->key);
			}
			// Failures are logged by group name inside, for the same reason.
			/* A job a group queues reports a later failure to whoever wrote a key of that group:
			   each writer once, a menu as box and a network caller that names none as remote.
			   So the groups run one by one, each in its writers' scope, in the order applyBatch
			   would run them. */
			std::vector<const ApplyGroup *> ran;
			for (size_t i = 0; i < grouped.size(); ++i)
			{
				const ApplyGroup *g = groupOf(grouped[i]);
				if (std::find(ran.begin(), ran.end(), g) != ran.end())
					continue;
				ran.push_back(g);
				std::vector<std::string> keys;
				for (size_t k = i; k < grouped.size(); ++k)
					if (groupOf(grouped[k]) == g)
						keys.push_back(grouped[k]);
				std::string who;
				for (size_t t = 0; t < taken.size(); ++t)
				{
					if (taken[t].row->needs_restart || groupOf(taken[t].row->key) != g)
						continue;
					const std::string one = taken[t].on_loop ? std::string("box") :
						(taken[t].writer.empty() ? std::string("remote") : taken[t].writer);
					if ((" " + who + " ").find(" " + one + " ") == std::string::npos)
						who += (who.empty() ? "" : " ") + one;
				}
				ApplyInitiatorScope scope(who);
				applyBatch(keys);
			}

			announce(taken);
		}

	private:
		struct Write;

		/* Tells the loop which settings landed, after they were applied: a menu open
		   on the box holds some of them, and one that kept showing the old value
		   would write it back with its next change. Posted rather than called, so
		   the screens hear of it in their own turn and this layer reaches no
		   screen. A refused post leaves a menu showing an old value until it is
		   drawn again, which is reported and not retried. */
		static void announce(const std::vector<Write> &taken)
		{
			std::string keys;
			for (size_t i = 0; i < taken.size(); ++i)
			{
				const std::string k(taken[i].row->key);
				if (("\n" + keys).find("\n" + k + "\n") != std::string::npos)
					continue;
				keys += k + "\n";
			}
			if (keys.empty())
				return;
			if (!postPayload(NeutrinoMessages::EVT_SETTINGS_WRITTEN, keys.c_str(), keys.size() + 1).ok())
				std::fprintf(stderr, "coreapi: the loop was not told which settings were written\n");
		}

		struct Write
		{
			const Descriptor *row;
			long              number;
			std::string       text;
			std::vector<std::string>  list;
			std::vector<RecordValues> records;
			// Which post promised this write to the loop. Nought is one nobody
			// has promised, and a post that fails takes its own back by it.
			unsigned          batch;
			// Who wrote it, so that a post promises what its own caller wrote
			// and a refused one takes back no more than that.
			pthread_t         owner;
			// Written by the box's own loop, a menu, rather than over the network.
			bool              on_loop;
			// The writer the write was made for, see currentWriter().
			std::string       writer;

			Write() : row(0), number(0), batch(0), owner(pthread_self()), on_loop(onApplyLoop()),
				  writer(currentWriter()) {}
		};

		/* Linear over the table, read once per call and a few hundred rows: what an index would
		   save is below what building it costs. The one table this layer declares, rather than one
		   handed in beside the values, so the lookup that finds the key and the lookup that finds
		   its field are the same table. */
		const Descriptor *find(const char *key) const
		{
			if (!values || key == 0)
				return 0;
			const Descriptor *table = settingsTable();
			const size_t count = settingsTableCount();
			for (size_t i = 0; i < count; ++i)
			{
				if (std::strcmp(table[i].key, key) == 0)
					return &table[i];
			}
			return 0;
		}

		/* Promises to the loop every write the caller holds that nobody has promised yet, under
		   a new stamp. Caller holds the lock. */
		unsigned stampMine(pthread_t mine)
		{
			// Nought is the mark of a write nobody has promised, so the
			// counter steps over it where it wraps.
			unsigned batch = stamp + 1;
			if (batch == 0)
				batch = 1;
			stamp = batch;
			for (size_t i = 0; i < held.size(); ++i)
			{
				if (held[i].batch == 0 && pthread_equal(held[i].owner, mine))
					held[i].batch = batch;
			}
			return batch;
		}

		// The last write of a setting is the one that counts, so the search runs
		// from the end. Caller holds the lock.
		const Write *latest(const Descriptor *row) const
		{
			for (size_t i = held.size(); i > 0; --i)
			{
				if (held[i - 1].row == row)
					return &held[i - 1];
			}
			return 0;
		}

		/* Who is asking. The thread, because that is what a caller is here: the layer above reaches
		   this on the thread its request runs on. Asked of the system rather than of the thread
		   wrapper beside it, which answers with nothing for a thread it did not start, and the
		   callers here are not its. */
		static pthread_t caller()
		{
			return pthread_self();
		}

		// Why the lookup found nothing: a source with nothing to read is not the
		// same answer as a key nothing declares, and only the first is a fault.
		Status status(const Descriptor *) const
		{
			if (!values)
				return Status::Internal;
			return Status::NotFound;
		}

		SNeutrinoSettings *values;
		bool             (*save)();

		// What the last post stamped its writes with. Only ever read and raised
		// under the lock.
		unsigned                    stamp;

		std::vector<Write>          held;
		mutable OpenThreads::Mutex  guard;
};

RealSettingsSource g_real_settings;

} // anonymous namespace

void installRealSettingsSource(SNeutrinoSettings *values, bool (*save)())
{
	g_real_settings.bind(values, save);
	setSettingsSource(&g_real_settings);
}

void applyPendingSettings() { g_real_settings.applyAndSave(); }

} // namespace coreapi
