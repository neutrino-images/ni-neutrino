// A menu loop whose clearing on one line closes with its brace, so the run after it is outside.
int CMenuWidget::exec(CMenuTarget *parent, const std::string &)
{
	WaitingMenu waiting_here(this);
	int rv;
	{ WaitingMenu busy(NULL); rv = it->second.menue->exec(this, it->second.action); }
	rv = item->exec(this);
	return rv;
}
