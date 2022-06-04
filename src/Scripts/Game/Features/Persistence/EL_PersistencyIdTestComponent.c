class EL_PersistencyIdTestComponentClass : ScriptComponentClass
{
}

class EL_PersistencyIdTestComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "GUID to idendentify map objects in persistency data", category: "Persistency")]
	protected string m_PersistencyID;
	
	override event void _WB_OnInit(IEntity owner, inout vector mat[4], IEntitySource src)
	{
		super._WB_OnInit(owner, mat, src);
		
		//Persistency ID alraedy set
		if(m_PersistencyID) return;
		
		WorldEditorAPI worldEditorApi = GenericEntity.Cast(owner)._WB_GetEditorAPI();
		if(!worldEditorApi) return;

		IEntitySource entitySource = worldEditorApi.EntityToSource(owner);
		if (!entitySource) return;
		
		string guid = Workbench.GenerateGloballyUniqueID64();

		worldEditorApi.SetVariableValue(entitySource, {new ContainerIdPathEntry(ClassName())}, "m_PersistencyID", guid);
	}
}
