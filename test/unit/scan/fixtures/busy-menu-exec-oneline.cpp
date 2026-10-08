// A menu loop that clears the menu on top on the line of the run, for each way to write the call.
int CMenuWidget::exec(CMenuTarget *parent, const std::string &)
{
	WaitingMenu waiting_here(this);
	int rv;
	{ WaitingMenu busy(NULL); rv = it->second.menue->exec(this, it->second.action); }
	{ WaitingMenu busy(NULL); rv = item.exec(this); }
	{ WaitingMenu busy(NULL); rv = exec(item); }
	return rv;
}
