modded class SCR_LightsTurnLeftUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		Print("SCR_LightsTurnLeftUserAction::PerformAction");
		
		if(Replication.IsServer())
		{
			Print("Nope ...");
			return;
		}
		
		super.PerformAction(pOwnerEntity, pUserEntity);
	}
}
