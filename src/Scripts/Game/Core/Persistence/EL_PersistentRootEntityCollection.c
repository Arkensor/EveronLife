[EL_DbName(EL_PersistentRootEntityCollection, "RootEntityCollection")]
class EL_PersistentRootEntityCollection : EL_MetaDataDbEntity
{
	ref set<string> m_aRemovedBackedEntities = new set<string>();
	ref map<typename, ref set<string>> m_mSelfSpawnDynamicEntities = new map<typename, ref set<string>>();

	//------------------------------------------------------------------------------------------------
	void Add(EL_PersistenceComponent persistenceComponent, bool baked, bool forceSelfSpawn = false)
	{
		string persistentId = persistenceComponent.GetPersistentId();

		int idx = m_aRemovedBackedEntities.Find(persistentId);
		if (idx != -1) m_aRemovedBackedEntities.Remove(idx);

		EL_PersistenceComponentClass settings = EL_PersistenceComponentClass.Cast(persistenceComponent.GetComponentData(persistenceComponent.GetOwner()));
		if (!baked && settings.m_bSelfSpawn || forceSelfSpawn)
		{
			set<string> ids = m_mSelfSpawnDynamicEntities.Get(settings.m_tSaveDataTypename);

			if (!ids)
			{
				ids = new set<string>();
				m_mSelfSpawnDynamicEntities.Set(settings.m_tSaveDataTypename, ids);
			}

			ids.Insert(persistentId);
		}
	}

	//------------------------------------------------------------------------------------------------
	void Remove(EL_PersistenceComponent persistenceComponent, bool baked)
	{
		string persistentId = persistenceComponent.GetPersistentId();

		if (baked)
		{
			m_aRemovedBackedEntities.Insert(persistentId);
			return;
		}

		EL_PersistenceComponentClass settings = EL_PersistenceComponentClass.Cast(persistenceComponent.GetComponentData(persistenceComponent.GetOwner()));
		set<string> ids = m_mSelfSpawnDynamicEntities.Get(settings.m_tSaveDataTypename);
		if (!ids) return;

		int idx = ids.Find(persistentId);
		if (idx != -1) ids.Remove(idx);
	}

	//------------------------------------------------------------------------------------------------
	void Save(EL_DbContext dbContext)
	{
		// Remove collection if it only holds default values
		if (m_aRemovedBackedEntities.IsEmpty() && m_mSelfSpawnDynamicEntities.IsEmpty())
		{
			// Only need to call db if it was previously saved (aka it has an id)
			if (HasId()) dbContext.RemoveAsync(this);
		}
		else
		{
			dbContext.AddOrUpdateAsync(this);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool SerializationSave(BaseSerializationSaveContext saveContext)
	{
		if (!saveContext.IsValid()) return false;

		SerializeMetaData(saveContext);
		
		saveContext.WriteValue("m_aRemovedBackedEntities", m_aRemovedBackedEntities);

		array<ref EL_SelfSpawnDynamicEntity> selfSpawnDynamicEntities();
		selfSpawnDynamicEntities.Reserve(m_mSelfSpawnDynamicEntities.Count());

		foreach (typename saveDataType, set<string> ids : m_mSelfSpawnDynamicEntities)
		{
			EL_SelfSpawnDynamicEntity entry();
			entry.m_sSaveDataType = EL_DbName.Get(saveDataType);
			entry.m_aIds = ids;
			selfSpawnDynamicEntities.Insert(entry);
		}

		saveContext.WriteValue("m_aSelfSpawnDynamicEntities", selfSpawnDynamicEntities);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool SerializationLoad(BaseSerializationLoadContext loadContext)
	{
		if (!loadContext.IsValid()) return false;

		DeserializeMetaData(loadContext);

		loadContext.ReadValue("m_aRemovedBackedEntities", m_aRemovedBackedEntities);

		array<ref EL_SelfSpawnDynamicEntity> selfSpawnDynamicEntities();
		loadContext.ReadValue("m_aSelfSpawnDynamicEntities", selfSpawnDynamicEntities);

		foreach (EL_SelfSpawnDynamicEntity entry : selfSpawnDynamicEntities)
		{
			m_mSelfSpawnDynamicEntities.Set(EL_DbName.GetTypeByName(entry.m_sSaveDataType), entry.m_aIds);
		}

		return true;
	}
}

class EL_SelfSpawnDynamicEntity
{
	string m_sSaveDataType;
	ref set<string> m_aIds;
}
