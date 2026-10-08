// A menu loop that runs an item with no clearing of the menu on top.
int CMenuWidget::exec(CMenuTarget *parent, const std::string &)
{
	WaitingMenu waiting_here(this);
	int rv;
	{
		WaitingMenu busy(NULL);
		rv = it->second.menue->exec(this, it->second.action);
	}
	rv = item->exec(this);
	return rv;
}
