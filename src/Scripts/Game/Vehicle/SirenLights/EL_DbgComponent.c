class EL_DbgComponentClass : ScriptComponentClass
{

}

class EL_DbgComponent : ScriptComponent
{
	override void EOnInit(IEntity owner)
	{
		PrintFormat("EOnInit(%1)", owner);
	}

	override void EOnVisible(IEntity owner, int frameNumber)
	{
		PrintFormat("EOnInit(%1, %2)", owner, frameNumber);
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		PrintFormat("EOnFrame(%1, %2)", owner, timeSlice);
	}

	override void EOnPostFrame(IEntity owner, float timeSlice)
	{
		PrintFormat("EOnPostFrame(%1, %2)", owner, timeSlice);
	}

	override void EOnSimulate(IEntity owner, float timeSlice)
	{
		PrintFormat("EOnSimulate(%1, %2)", owner, timeSlice);
	}

	override void EOnPostSimulate(IEntity owner, float timeSlice)
	{
		PrintFormat("EOnPostSimulate(%1, %2)", owner, timeSlice);
	}

	override void EOnFixedFrame(IEntity owner, float timeSlice)
	{
		PrintFormat("EOnFixedFrame(%1, %2)", owner, timeSlice);
	}

	override void EOnPostFixedFrame(IEntity owner, float timeSlice)
	{
		PrintFormat("EOnPostFixedFrame(%1, %2)", owner, timeSlice);
	}

	override void OnPostInit(IEntity owner)
	{
		PrintFormat("OnPostInit(%1)", owner);
		
		SetEventMask(owner, EntityEvent.SIMULATE | EntityEvent.FRAME | EntityEvent.FIXEDFRAME);
		
		EntityEvent events = owner.GetEventMask();
		
		Print("EntityEvent.TOUCH: " + (events & EntityEvent.TOUCH));
		Print("EntityEvent.INIT: " + (events & EntityEvent.INIT));
		Print("EntityEvent.VISIBLE: " + (events & EntityEvent.VISIBLE));
		Print("EntityEvent.FRAME: " + (events & EntityEvent.FRAME));
		Print("EntityEvent.POSTFRAME: " + (events & EntityEvent.POSTFRAME));
		Print("EntityEvent.ANIMEVENT: " + (events & EntityEvent.ANIMEVENT));
		Print("EntityEvent.SIMULATE: " + (events & EntityEvent.SIMULATE));
		Print("EntityEvent.POSTSIMULATE: " + (events & EntityEvent.POSTSIMULATE));
		Print("EntityEvent.JOINTBREAK: " + (events & EntityEvent.JOINTBREAK));
		Print("EntityEvent.PHYSICSMOVE: " + (events & EntityEvent.PHYSICSMOVE));
		Print("EntityEvent.CONTACT: " + (events & EntityEvent.CONTACT));
		Print("EntityEvent.PHYSICSACTIVE: " + (events & EntityEvent.PHYSICSACTIVE));
		Print("EntityEvent.DIAG: " + (events & EntityEvent.DIAG));
		Print("EntityEvent.FIXEDFRAME: " + (events & EntityEvent.FIXEDFRAME));
		Print("EntityEvent.POSTFIXEDFRAME: " + (events & EntityEvent.POSTFIXEDFRAME));
		
		// todo test if we enable component event if the entity has it too
	}
}