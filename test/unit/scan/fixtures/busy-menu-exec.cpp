// A menu loop that clears the menu on top around both item runs.
int CMenuWidget::exec(CMenuTarget *parent, const std::string &)
{
	WaitingMenu waiting_here(this);
	int rv;
	{
		WaitingMenu busy(NULL);
		rv = it->second.menue->exec(this, it->second.action);
	}
	if (handled)
	{
		WaitingMenu busy(NULL);
		rv = item->exec(this);
	}
	return rv;
}
