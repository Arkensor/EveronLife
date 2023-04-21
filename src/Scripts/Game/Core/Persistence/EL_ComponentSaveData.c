[BaseContainerProps()]
class EL_ComponentSaveDataClass
{
	//------------------------------------------------------------------------------------------------
	array<typename> Requires(); // TODO: Implement automatic entity source changes on prefab edit.

	//------------------------------------------------------------------------------------------------
	//array<typename> CannotCombine(); // TODO: Implement
}

class EL_ComponentSaveData
{
	int m_iDataLayoutVersion = 1;

	//------------------------------------------------------------------------------------------------
	//! Reads the save-data from the world entity component
	//! \param worldEntityComponent the component to read the save-data from
	//! \param attributes the class-class shared configuration attributes assigned in the world editor
	//! \return true if save-data could be read, false if something failed.
	bool ReadFrom(notnull GenericComponent worldEntityComponent, notnull EL_ComponentSaveDataClass attributes)
	{
		return EL_DbEntityUtils.StructAutoCopy(worldEntityComponent, this);
	}

	//------------------------------------------------------------------------------------------------
	//! Compares the save-data against a world entity component to find out which save-data belongs to with component in case there are multiple instances of the component present (e.g. storages).
	//! \param worldEntityComponent the component to compare against
	//! \param attributes the class-class shared configuration attributes assigned in the world editor
	//! \return true if the component is the one the save-data was meant for, false otherwise.
	bool IsFor(notnull GenericComponent worldEntityComponent, notnull EL_ComponentSaveDataClass attributes)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the save-data to the world entity component
	//! \param worldEntityComponent the component to apply the save-data to
	//! \param attributes the class-class shared configuration attributes assigned in the world editor
	//! \return true if save-data could be applied, false if something failed.
	bool ApplyTo(notnull GenericComponent worldEntityComponent, notnull EL_ComponentSaveDataClass attributes)
	{
		return EL_DbEntityUtils.StructAutoCopy(this, worldEntityComponent);
	}
}

class EL_ComponentSaveDataType : BaseContainerCustomTitle
{
	static ref map<typename, typename> s_mMapping;

	#ifdef WORKBENCH
	protected string m_sWorkbenchTitle;
	#endif

	//------------------------------------------------------------------------------------------------
	static typename Get(typename saveDataType)
	{
		if (!s_mMapping) return typename.Empty;

		return s_mMapping.Get(saveDataType);
	}

	//------------------------------------------------------------------------------------------------
	void EL_ComponentSaveDataType(typename saveDataType, typename componentType)
	{
		if (!saveDataType.IsInherited(EL_ComponentSaveDataClass))
		{
			Debug.Error(string.Format("Failed to register '%1' as persistence save-data type for '%2'. '%1' must inherit from '%3'.", saveDataType, componentType, EL_ComponentSaveDataClass));
		}

		if (!saveDataType.ToString().EndsWith("Class"))
		{
			Debug.Error(string.Format("Failed to register '%1' as persistence save-data type for '%2'. '%1' must follow xyzClass naming convention.", saveDataType));
		}

		if (!s_mMapping) s_mMapping = new map<typename, typename>();

		s_mMapping.Set(saveDataType, componentType);

		#ifdef WORKBENCH
		m_sWorkbenchTitle = componentType.ToString();
		#endif
	}

	#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	override bool _WB_GetCustomTitle(BaseContainer source, out string title)
	{
		title = m_sWorkbenchTitle;
		return true;
	}
	#endif
}