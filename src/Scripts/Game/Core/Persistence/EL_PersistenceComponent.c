[ComponentEditorProps(category: "EveronLife/Core/Persistence", description: "Used to make an entity persistent.")]
sealed class EL_PersistenceComponentClass : ScriptComponentClass
{
	[Attribute(defvalue: EL_ESaveType.INTERVAL_SHUTDOWN.ToString(), uiwidget: UIWidgets.ComboBox, desc: "Should the entity be saved automatically and if so only on shutdown or regulary.\nThe interval is configured in the persitence manager component on your game mode.", enums: ParamEnumArray.FromEnum(EL_ESaveType))]
	EL_ESaveType m_eSaveType;

	[Attribute(defvalue: "0", desc: "If enabled a copy of the last save-data is kept to compare against, so the databse is updated only if there are any differences to what is already persisted.\nHelps to reduce expensive database calls at the cost of additional base line memeory allocation.")]
	bool m_bUseChangeTracker;

	[Attribute(defvalue: "1", desc: "If enabled the entity will spawn back into the world automatically after session restart.\nAlways true for baked map objects.")]
	bool m_bSelfSpawn;

	[Attribute(defvalue: "1", desc: "If enabled the entity will deleted from persistence when deleted from the world automatically.")]
	bool m_bSelfDelete;

	[Attribute(defvalue: "1", desc: "Only storage root entities can be saved in the open world.\nIf disabled the entity will only be saved if inside another storage root (e.g. character, vehicle).")]
	bool m_bStorageRoot;

	[Attribute(desc: "Type of save-data to represent this entity.")]
	ref EL_EntitySaveDataClass m_pSaveData;

	// Derived from shared initialization
	typename m_tSaveDataTypename;

	//------------------------------------------------------------------------------------------------
	static override bool DependsOn(string className)
	{
		return true; // Required for child prefab EOnInit order somehow ...
	}

	//------------------------------------------------------------------------------------------------
	static override array<typename> CannotCombine(IEntityComponentSource src)
	{
		return {EL_PersistenceComponent}; //Prevent multiple persistence components from being added.
	}
};

sealed class EL_PersistenceComponent : ScriptComponent
{
	private string m_sId;

	private int m_iLastSaved;

	[NonSerialized()]
	private EL_EPersistenceFlags m_eFlags;

	[NonSerialized()]
	private ref ScriptInvoker<EL_PersistenceComponent, EL_EntitySaveData> m_pOnAfterSave;

	[NonSerialized()]
	private ref ScriptInvoker<EL_PersistenceComponent, EL_EntitySaveData> m_pOnAfterPersist;

	[NonSerialized()]
	private ref ScriptInvoker<EL_PersistenceComponent, EL_EntitySaveData> m_pOnBeforeLoad;

	[NonSerialized()]
	private ref ScriptInvoker<EL_PersistenceComponent, EL_EntitySaveData> m_pOnAfterLoad;

	[NonSerialized()]
	private static ref map<EL_PersistenceComponent, ref EL_EntitySaveData> m_mLastSaveData;

	//------------------------------------------------------------------------------------------------
	//! static helper see GetPersistentId()
	static string GetPersistentId(IEntity entity)
	{
		if (!entity) return string.Empty;
		EL_PersistenceComponent persistence = EL_PersistenceComponent.Cast(entity.FindComponent(EL_PersistenceComponent));
		if (!persistence) return string.Empty;
		return persistence.GetPersistentId();
	}

	//------------------------------------------------------------------------------------------------
	//! Get the assigned persistent id of this entity.
	string GetPersistentId()
	{
		if (!m_sId) m_sId = EL_PersistenceManager.GetInstance().Register(this);
		return m_sId;
	}

	//------------------------------------------------------------------------------------------------
	//! Set the assigned persistent id of this entity.
	//! USE WITH CAUTION! Only in rare situations you need to manually assign an id.
	void SetPersistentId(string id)
	{
		EL_PersistenceManager persistenceManager = EL_PersistenceManager.GetInstance();
		if (m_sId && m_sId != id)
		{
			persistenceManager.Unregister(this);
			m_sId = string.Empty;
		}
		if (!m_sId) m_sId = persistenceManager.Register(this, id);
	}

	//------------------------------------------------------------------------------------------------
	//! Get the last time this entity was saved as UTC unix timestamp
	int GetLastSaved()
	{
		return m_iLastSaved;
	}

	//------------------------------------------------------------------------------------------------
	//! Get internal state flags of the persistence tracking
	EL_EPersistenceFlags GetFlags()
	{
		return m_eFlags;
	}

	//------------------------------------------------------------------------------------------------
	//! Event invoker for when the save-data was read but was not yet persisted to the database.
	//! Args(EL_PersistenceComponent, EL_EntitySaveData)
	ScriptInvoker GetOnAfterSaveEvent()
	{
		if (!m_pOnAfterSave) m_pOnAfterSave = new ScriptInvoker();
		return m_pOnAfterSave;
	}

	//------------------------------------------------------------------------------------------------
	//! Event invoker for when the save-data was persisted to the database.
	//! Only called on world root entities (e.g. not on items stored inside other items, there it will only be called for the container).
	//! Args(EL_PersistenceComponent, EL_EntitySaveData)
	ScriptInvoker GetOnAfterPersistEvent()
	{
		if (!m_pOnAfterPersist) m_pOnAfterPersist = new ScriptInvoker();
		return m_pOnAfterPersist;
	}

	//------------------------------------------------------------------------------------------------
	//! Event invoker for when the save-data is about to be loaded/applied to the entity.
	//! Args(EL_PersistenceComponent, EL_EntitySaveData)
	ScriptInvoker GetOnBeforeLoadEvent()
	{
		if (!m_pOnBeforeLoad) m_pOnBeforeLoad = new ScriptInvoker();
		return m_pOnBeforeLoad;
	}

	//------------------------------------------------------------------------------------------------
	//! Event invoker for when the save-data was loaded/applied to the entity.
	//! Args(EL_PersistenceComponent, EL_EntitySaveData)
	ScriptInvoker GetOnAfterLoadEvent()
	{
		if (!m_pOnAfterLoad) m_pOnAfterLoad = new ScriptInvoker();
		return m_pOnAfterLoad;
	}

	//------------------------------------------------------------------------------------------------
	//! Pause automated tracking for auto-/shutdown-save, root entity changes and removal.
	//! Used primarily to handle the conditional removal of an entity manually. E.g. pause before virtually storing a vehicle in a garage (during which the entity gets deleted).
	void PauseTracking()
	{
		EL_BitFlags.SetFlags(m_eFlags, EL_EPersistenceFlags.PAUSE_TRACKING);
	}

	//------------------------------------------------------------------------------------------------
	//! Undo PauseTracking().
	void ResumeTracking()
	{
		EL_BitFlags.ClearFlags(m_eFlags, EL_EPersistenceFlags.PAUSE_TRACKING);
	}

	//------------------------------------------------------------------------------------------------
	//! Save the entity to the database
	//! \return the save-data instance that was submitted to the database
	EL_EntitySaveData Save(out EL_EReadResult readResult = EL_EReadResult.ERROR)
	{
		GetPersistentId(); // Make sure the id has been assigned

		m_iLastSaved = System.GetUnixTime();

		IEntity owner = GetOwner();
		EL_PersistenceComponentClass settings = EL_PersistenceComponentClass.Cast(GetComponentData(owner));
		EL_EntitySaveData saveData = EL_EntitySaveData.Cast(settings.m_tSaveDataTypename.Spawn());
		if (saveData)
			readResult = saveData.ReadFrom(owner, settings.m_pSaveData);

		if (!readResult)
		{
			Debug.Error(string.Format("Failed to persist world entity '%1'@%2. Save-data could not be read.",
				EL_Utils.GetPrefabName(owner),
				owner.GetOrigin()));
			return null;
		}

		if (m_pOnAfterSave)
			m_pOnAfterSave.Invoke(this, saveData);

		EL_PersistenceManager persistenceManager = EL_PersistenceManager.GetInstance();

		bool isPersistent = EL_BitFlags.CheckFlags(m_eFlags, EL_EPersistenceFlags.PERSISTENT_RECORD);

		// Save root entities unless they are baked AND only have default values,
		// cause then we do not need the record to restore - as the ids will be
		// known through the name mapping table instead.
		bool wasPersisted;
		if (EL_BitFlags.CheckFlags(m_eFlags, EL_EPersistenceFlags.ROOT) &&
			(!EL_BitFlags.CheckFlags(m_eFlags, EL_EPersistenceFlags.BAKED) || readResult == EL_EReadResult.OK))
		{
			// Check if the update is really needed
			EL_EntitySaveData lastData;
			if (settings.m_bUseChangeTracker)
				lastData = m_mLastSaveData.Get(this);

			if (!isPersistent || !lastData || !lastData.Equals(saveData))
			{
				persistenceManager.AddOrUpdateAsync(saveData);
				EL_BitFlags.SetFlags(m_eFlags, EL_EPersistenceFlags.PERSISTENT_RECORD);
				wasPersisted = true;
			}
		}
		else if (isPersistent)
		{
			// Was previously saved as storage root but now is not anymore, so the toplevel db entry has to be deleted.
			// The save-data will be present inside the storage parent instead.
			persistenceManager.RemoveAsync(settings.m_tSaveDataTypename, GetPersistentId());
			EL_BitFlags.ClearFlags(m_eFlags, EL_EPersistenceFlags.PERSISTENT_RECORD);
		}

		if (settings.m_bUseChangeTracker)
			m_mLastSaveData.Set(this, saveData);

		if (m_pOnAfterPersist && wasPersisted)
			m_pOnAfterPersist.Invoke(this, saveData);

		return saveData;
	}

	//------------------------------------------------------------------------------------------------
	//! Load existing save-data to apply to this entity
	//! \param saveData existing data to restore the entity state from
	//! \param isRoot true if the current entity is a world root (not a stored item inside a storage)
	bool Load(notnull EL_EntitySaveData saveData, bool isRoot = true)
	{
		if (m_pOnBeforeLoad)
			m_pOnBeforeLoad.Invoke(this, saveData);

		if (isRoot)
		{
			// Restore information if this entity has its own root record in db to avoid unreferenced junk entries.
			EL_BitFlags.SetFlags(m_eFlags, EL_EPersistenceFlags.PERSISTENT_RECORD);
		}
		else
		{
			// Supress registration as root to reduce map interactions in manager when entity is added as child next call anyway.
			EL_BitFlags.ClearFlags(m_eFlags, EL_EPersistenceFlags.ROOT);
		}

		// Restore transform info relevant for baked entity record trimming
		if (EL_BitFlags.CheckFlags(m_eFlags, EL_EPersistenceFlags.BAKED) &&
			saveData.m_pTransformation &&
			!saveData.m_pTransformation.IsDefault())
		{
			FlagAsMoved();
		}

		SetPersistentId(saveData.GetId());

		IEntity owner = GetOwner();
		EL_PersistenceComponentClass settings = EL_PersistenceComponentClass.Cast(GetComponentData(owner));
		EL_EApplyResult applyResult = saveData.ApplyTo(owner, settings.m_pSaveData);
		if (applyResult == EL_EApplyResult.ERROR)
		{
			Debug.Error(string.Format("Failed to apply save-data '%1:%2' to entity.", saveData.Type().ToString(), saveData.GetId()));
			return false;
		}

		if (settings.m_bUseChangeTracker)
			m_mLastSaveData.Set(this, saveData);

		if (applyResult == EL_EApplyResult.AWAIT_COMPLETION)
		{
			EL_DeferredApplyResult.GetOnApplied(saveData).Insert(DeferredApplyCallback)
		}
		else
		{
			if (m_pOnAfterLoad)
				m_pOnAfterLoad.Invoke(this, saveData);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Mark the persistence data of this entity for deletion. Does not delete the entity itself.
	//! Note: Will be immediate on EL_ESaveType.MANUAL, otherwise happens on auto/shutdown save.
	void Delete()
	{
		// Only attempt to delete if there is a chance it was already saved as own entity in db
		if (m_sId && EL_BitFlags.CheckFlags(m_eFlags, EL_EPersistenceFlags.PERSISTENT_RECORD))
		{
			EL_BitFlags.ClearFlags(m_eFlags, EL_EPersistenceFlags.PERSISTENT_RECORD);
			EL_PersistenceManager persistenceManager = EL_PersistenceManager.GetInstance();
			EL_PersistenceComponentClass settings = EL_PersistenceComponentClass.Cast(GetComponentData(GetOwner()));
			persistenceManager.EnqueueRemoval(settings.m_tSaveDataTypename, m_sId, settings.m_eSaveType);
		}

		m_sId = string.Empty;
		m_iLastSaved = 0;
	}

	//------------------------------------------------------------------------------------------------
	override protected event void OnPostInit(IEntity owner)
	{
		// Persistence logic only runs on the server
		if (!EL_PersistenceManager.IsPersistenceMaster()) return;

		// Init and validate settings on shared class-class instance once
		EL_PersistenceComponentClass settings = EL_PersistenceComponentClass.Cast(GetComponentData(owner));
		if (!settings.m_tSaveDataTypename)
		{
			if (!settings.m_pSaveData || settings.m_pSaveData.Type() == EL_EntitySaveDataClass)
			{
				Debug.Error(string.Format("Missing or invalid save-data type in persistence component on entity '%1'@%2. Entity will not be persisted!",
					EL_Utils.GetPrefabName(owner),
					owner.GetOrigin()));
				return;
			}

			settings.m_tSaveDataTypename = EL_Utils.TrimEnd(settings.m_pSaveData.ClassName(), 5).ToType();

			// Collect and validate component save data types
			array<typename> componentSaveDataTypes();
			componentSaveDataTypes.Reserve(settings.m_pSaveData.m_aComponents.Count());
			foreach (EL_ComponentSaveDataClass componentSaveData : settings.m_pSaveData.m_aComponents)
			{
				if (!componentSaveData.m_bEnabled)
					continue;

				typename componentSaveDataType = componentSaveData.Type();

				if (!componentSaveDataType || componentSaveDataType == EL_ComponentSaveDataClass)
				{
					Debug.Error(string.Format("Invalid save-data type '%1' in persistence component on entity '%2'@%3. Associated component data will not be persisted!",
						componentSaveDataType,
						EL_Utils.GetPrefabName(owner),
						owner.GetOrigin()));
					continue;
				}

				componentSaveDataTypes.Insert(componentSaveDataType);
			}

			// Re-order save data class-classes in attribute instance by inheritance
			array<ref EL_ComponentSaveDataClass> sortedComponents();
			sortedComponents.Reserve(settings.m_pSaveData.m_aComponents.Count());
			foreach (typename componentType : EL_Utils.SortTypenameHierarchy(componentSaveDataTypes))
			{
				foreach (EL_ComponentSaveDataClass componentSaveData : settings.m_pSaveData.m_aComponents)
				{
					if (componentSaveData.Type() == componentType) sortedComponents.Insert(componentSaveData);
				}
			}
			settings.m_pSaveData.m_aComponents = sortedComponents;

			if (settings.m_bUseChangeTracker && !m_mLastSaveData)
				m_mLastSaveData = new map<EL_PersistenceComponent, ref EL_EntitySaveData>();
		}

		SetEventMask(owner, EntityEvent.INIT);

		if (settings.m_bStorageRoot)
			EL_BitFlags.SetFlags(m_eFlags, EL_EPersistenceFlags.ROOT);

		EL_PersistenceManager persistenceManager = EL_PersistenceManager.GetInstance();
		if (persistenceManager.GetState() < EL_EPersistenceManagerState.SETUP)
			EL_BitFlags.SetFlags(m_eFlags, EL_EPersistenceFlags.BAKED);

		persistenceManager.EnqueueRegistration(this);

		bool isChar = ChimeraCharacter.Cast(owner);
		bool isTurret = EL_Component<TurretControllerComponent>.Find(owner);
		if (isChar || isTurret)
		{
			// Weapon/Turret selected flag
			EventHandlerManagerComponent eventHandler = EL_Component<EventHandlerManagerComponent>.Find(owner);
			if (eventHandler)
				eventHandler.RegisterScriptHandler("OnWeaponChanged", this, OnWeaponChanged, delayed: false);
		}
		if (isChar)
		{
			// Gadget selected flag
			SCR_CharacterControllerComponent characterController = EL_Component<SCR_CharacterControllerComponent>.Find(owner);
			characterController.m_OnGadgetStateChangedInvoker.Insert(OnGadgetStateChanged);
		}

		// For vehicles we want to get notified when they encounter their first contact or start to be driven
		if (settings.m_pSaveData.m_bTrimDefaults && EL_Component<VehicleControllerComponent>.Find(owner))
		{
			SetEventMask(owner, EntityEvent.CONTACT);
			EventHandlerManagerComponent ev = EL_Component<EventHandlerManagerComponent>.Find(owner);
			if (ev) ev.RegisterScriptHandler("OnCompartmentEntered", this, OnCompartmentEntered);
		}
	}

	//------------------------------------------------------------------------------------------------
	override protected void EOnInit(IEntity owner)
	{
		EL_BitFlags.SetFlags(m_eFlags, EL_EPersistenceFlags.INITIALIZED);
		EL_DefaultPrefabItemsInfo.Finalize(owner);

		// TODO remove after https://feedback.bistudio.com/T172461 has been fixed
		IEntity parent = owner.GetParent();
		if (parent)
			OnAddedToParent(owner, parent);
	}

	//------------------------------------------------------------------------------------------------
	override event protected void OnAddedToParent(IEntity child, IEntity parent)
	{
		if (!EL_PersistenceManager.IsPersistenceMaster())
			return;

		// TODO: Remove workaround check after https://feedback.bistudio.com/T172461 was fixed
		if (EL_BitFlags.CheckFlags(m_eFlags, EL_EPersistenceFlags.HACK_PARENT_RAN))
			return;

		EL_BitFlags.SetFlags(m_eFlags, EL_EPersistenceFlags.HACK_PARENT_RAN);

		// TODO: Replace with subscribe to all parent slots after https://feedback.bistudio.com/T171945 is added.
		ResourceName prefab = EL_Utils.GetPrefabName(child);
		array<Managed> outComponents();
		parent.FindComponents(SlotManagerComponent, outComponents);
		foreach (Managed componentRef : outComponents)
		{
			SlotManagerComponent slotManager = SlotManagerComponent.Cast(componentRef);
			array<ref Tuple2<string, ResourceName>> slotinfos = EL_EntitySlotPrefabInfo.GetSlotInfos(parent, slotManager);
			foreach (Tuple2<string, ResourceName> slotInfo : slotinfos)
			{
				if (prefab == slotInfo.param2)
				{
					OnParentAdded();
					return;
				}
			}
		}
		parent.FindComponents(BaseSlotComponent, outComponents);
		foreach (Managed componentRef : outComponents)
		{
			if (prefab == EL_EntitySlotPrefabInfo.GetSlotPrefab(parent, BaseSlotComponent.Cast(componentRef)))
			{
				OnParentAdded();
				return;
			}
		}
		parent.FindComponents(WeaponSlotComponent, outComponents);
		foreach (Managed componentRef : outComponents)
		{
			if (prefab == EL_EntitySlotPrefabInfo.GetSlotPrefab(parent, WeaponSlotComponent.Cast(componentRef)))
			{
				OnParentAdded();
				return;
			}
		}

		InventoryItemComponent invItem = EL_Component<InventoryItemComponent>.Find(child);
		if (invItem)
			invItem.m_OnParentSlotChangedInvoker.Insert(OnParentSlotChanged);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnParentSlotChanged(InventoryStorageSlot oldSlot, InventoryStorageSlot newSlot)
	{
		OnParentAdded(newSlot);
	}

	//------------------------------------------------------------------------------------------------
	// TODO: Change signatur to entity slot so event can be reused with slot manager etc
	protected void OnParentAdded(InventoryStorageSlot newSlot = null)
	{
		IEntity owner = GetOwner();
		EL_BitFlags.ClearFlags(m_eFlags, EL_EPersistenceFlags.ROOT);

		if (newSlot)
		{
			EL_PersistenceComponent parentPersistence = EL_Component<EL_PersistenceComponent>.Find(newSlot.GetOwner());
			if (parentPersistence && !EL_BitFlags.CheckFlags(parentPersistence.GetFlags(), EL_EPersistenceFlags.INITIALIZED))
			{
				EL_DefaultPrefabItemsInfo.Add(owner, newSlot);
			}
			else
			{
				FlagAsMoved();
			}

			if (EL_Utils.IsAnyInherited(newSlot.GetStorage(), {EquipedLoadoutStorageComponent, BaseEquipmentStorageComponent, BaseEquipedWeaponStorageComponent}))
				EL_BitFlags.SetFlags(m_eFlags, EL_EPersistenceFlags.WAS_EQUIPPED);
		}
		else
		{
			StopMoveTracking();
		}

		if (m_sId && !EL_BitFlags.CheckFlags(m_eFlags, EL_EPersistenceFlags.PAUSE_TRACKING))
			EL_PersistenceManager.GetInstance().UpdateRootStatus(this, m_sId, EL_ComponentData<EL_PersistenceComponentClass>.Get(owner), false);

		// We only needed to know which slot we got attached to, so unsubscribe again
		InventoryItemComponent invItem = EL_Component<InventoryItemComponent>.Find(owner);
		if (invItem)
			invItem.m_OnParentSlotChangedInvoker.Remove(OnParentSlotChanged);

		// TODO: Also unsubscribe any entity slots

		EL_BitFlags.ClearFlags(m_eFlags, EL_EPersistenceFlags.HACK_PARENT_RAN);
	}

	//------------------------------------------------------------------------------------------------
	override event protected void OnRemovedFromParent(IEntity child, IEntity parent)
	{
		if (!EL_PersistenceManager.IsPersistenceMaster())
			return;

		EL_PersistenceComponentClass settings = EL_ComponentData<EL_PersistenceComponentClass>.Get(child);
		if (!settings.m_bStorageRoot)
			return;

		EL_BitFlags.SetFlags(m_eFlags, EL_EPersistenceFlags.ROOT);
		FlagAsMoved();

		if (m_sId && !EL_BitFlags.CheckFlags(m_eFlags, EL_EPersistenceFlags.PAUSE_TRACKING))
			EL_PersistenceManager.GetInstance().UpdateRootStatus(this, m_sId, settings, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnWeaponChanged(BaseWeaponComponent weapon, BaseWeaponComponent prevWeapon)
	{
		if (!weapon)
			return;

		IEntity weaponEntity = weapon.GetOwner();
		WeaponSlotComponent weaponSlot = WeaponSlotComponent.Cast(weapon);
		if (weaponSlot)
			weaponEntity = weaponSlot.GetWeaponEntity();

		EL_PersistenceComponent persistence = EL_Component<EL_PersistenceComponent>.Find(weaponEntity);
		if (persistence)
			persistence.FlagAsSelected();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnGadgetStateChanged(IEntity gadget, bool isInHand, bool isOnGround)
	{
		if (!gadget || !isInHand)
			return;

		EL_PersistenceComponent persistence = EL_Component<EL_PersistenceComponent>.Find(gadget);
		if (persistence)
			persistence.FlagAsSelected();
	}

	//------------------------------------------------------------------------------------------------
	protected event void OnCompartmentEntered(IEntity vehicle, BaseCompartmentManagerComponent mgr, IEntity occupant, int managerId, int slotID)
	{
		// Somebody entered the vehicle to potentially drive it, start listening to physics movements
		EventHandlerManagerComponent eventHandler = EL_Component<EventHandlerManagerComponent>.Find(vehicle);
		if (eventHandler)
			eventHandler.RemoveScriptHandler("OnCompartmentEntered", this, OnCompartmentEntered);

		SetEventMask(vehicle, EntityEvent.PHYSICSMOVE);
	}

	//------------------------------------------------------------------------------------------------
	override event protected void EOnContact(IEntity owner, IEntity other, Contact contact)
	{
		if (!GenericTerrainEntity.Cast(other))
			FlagAsMoved(); // Something moved the current vehicle via physics contact.
	}

	//------------------------------------------------------------------------------------------------
	override event protected void EOnPhysicsMove(IEntity owner)
	{
		// Check for if engine is one as there is tiny jitter movement during engine startup we want to ignore.
		VehicleControllerComponent vehicleController = EL_Component<VehicleControllerComponent>.Find(owner);
		if (vehicleController && vehicleController.IsEngineOn())
			FlagAsMoved();
	}

	//------------------------------------------------------------------------------------------------
	override event protected void OnDelete(IEntity owner)
	{
		if (m_mLastSaveData)
			m_mLastSaveData.Remove(this);

		// Check that we are not in session dtor phase
		EL_PersistenceManager persistenceManager = EL_PersistenceManager.GetInstance(false);
		if (!persistenceManager || persistenceManager.GetState() == EL_EPersistenceManagerState.SHUTDOWN)
			return;

		persistenceManager.Unregister(this);

		EL_PersistenceComponentClass settings = EL_PersistenceComponentClass.Cast(GetComponentData(owner));
		if (m_sId && !EL_BitFlags.CheckFlags(m_eFlags, EL_EPersistenceFlags.PAUSE_TRACKING))
		{
			persistenceManager.UpdateRootEntityCollection(this, m_sId, false);
			if (settings.m_bSelfDelete)
				Delete();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Get the instance of the save-data or component save-data class-class that was configured in the prefab/world editor.
	//! \param saveDataClassType typename of the entity or component save-data which should be returned
	//! \return instance of the save-data or null if not found
	Class GetAttributeClass(typename saveDataClassType)
	{
		EL_PersistenceComponentClass settings = EL_PersistenceComponentClass.Cast(GetComponentData(GetOwner()));
		if (!settings.m_pSaveData)
			return null;

		if (settings.m_pSaveData.IsInherited(saveDataClassType))
		{
			return settings.m_pSaveData;
		}
		else
		{
			// Find the first inheritance match. The typename array itereated is ordered by inheritance.
			foreach (EL_ComponentSaveDataClass componentSaveDataClass : settings.m_pSaveData.m_aComponents)
			{
				if (componentSaveDataClass.IsInherited(saveDataClassType)) return componentSaveDataClass;
			}
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Manually force the entity to be self-respawned even if the setting is disabled in prefab
	void ForceSelfSpawn()
	{
		EL_PersistenceManager.GetInstance().ForceSelfSpawn(this);
	}

	//------------------------------------------------------------------------------------------------
	void FlagAsSelected()
	{
		EL_BitFlags.SetFlags(m_eFlags, EL_EPersistenceFlags.WAS_SELECTED);
	}

	//------------------------------------------------------------------------------------------------
	void FlagAsBaked()
	{
		EL_BitFlags.SetFlags(m_eFlags, EL_EPersistenceFlags.BAKED);
	}

	//------------------------------------------------------------------------------------------------
	void FlagAsMoved()
	{
		StopMoveTracking();
		EL_BitFlags.SetFlags(m_eFlags, EL_EPersistenceFlags.WAS_MOVED);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopMoveTracking()
	{
		IEntity owner = GetOwner();
		ClearEventMask(owner, EntityEvent.CONTACT | EntityEvent.PHYSICSMOVE);
		EventHandlerManagerComponent eventHandler = EL_Component<EventHandlerManagerComponent>.Find(owner);
		if (eventHandler)
			eventHandler.RemoveScriptHandler("OnCompartmentEntered", this, OnCompartmentEntered);
	}

	//------------------------------------------------------------------------------------------------
	protected void DeferredApplyCallback(EL_EntitySaveData saveData)
	{
		if (m_pOnAfterLoad)
			m_pOnAfterLoad.Invoke(this, saveData);
	}

	#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	override event void _WB_OnInit(IEntity owner, inout vector mat[4], IEntitySource src)
	{
		super._WB_OnInit(owner, mat, src);

		if (owner.GetName())
			return;

		WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
		if (!worldEditor || worldEditor.IsPrefabEditMode())
			return;

		WorldEditorAPI worldEditorApi = GenericEntity.Cast(owner)._WB_GetEditorAPI();
		if (!worldEditorApi)
				return;

		worldEditorApi.RenameEntity(owner, GenerateName(owner));
	}

	//------------------------------------------------------------------------------------------------
	override array<ref WB_UIMenuItem> _WB_GetContextMenuItems(IEntity owner)
	{
		array<ref WB_UIMenuItem> items();

		if (EL_BaseSceneNameProxyEntity.GetProxyForBaseSceneEntity(owner))
		{
			items.Insert(new WB_UIMenuItem("Go to base scene name proxy", 0));
		}
		else if (!owner.GetName())
		{
			items.Insert(new WB_UIMenuItem("Spawn new base scene name proxy", 1));

			if (EL_BaseSceneNameProxyEntity.s_pSelectedProxy)
				items.Insert(new WB_UIMenuItem(string.Format("Assign proxy '%1'", EL_BaseSceneNameProxyEntity.s_pSelectedProxy.GetName()), 2));
		}

		return items;
	}

	//------------------------------------------------------------------------------------------------
	override void _WB_OnContextMenu(IEntity owner, int id)
	{
		WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
		if (!worldEditor || worldEditor.IsPrefabEditMode())
			return;

		WorldEditorAPI worldEditorApi = GenericEntity.Cast(owner)._WB_GetEditorAPI();
		if (!worldEditorApi)
				return;

		EL_BaseSceneNameProxyEntity nameProxy;

		if (id == 0)
		{
			nameProxy = EL_BaseSceneNameProxyEntity.GetProxyForBaseSceneEntity(owner);
		}
		else if (id == 2)
		{
			worldEditorApi.BeginEntityAction("BaseSceneNameProxyEntity__Assign");
			nameProxy = EL_BaseSceneNameProxyEntity.s_pSelectedProxy;
			EL_BaseSceneNameProxyEntity.s_pSelectedProxy = null;
			worldEditorApi.ModifyEntityKey(nameProxy, "coords", owner.GetOrigin().ToString(false));
			worldEditorApi.EndEntityAction();
		}
		else
		{
			worldEditorApi.BeginEntityAction("BaseSceneNameProxyEntity__Create");
			nameProxy = EL_BaseSceneNameProxyEntity.Cast(worldEditorApi.CreateEntity(
				"EL_BaseSceneNameProxyEntity",
				GenerateName(owner),
				worldEditorApi.GetCurrentEntityLayerId(),
				null,
				owner.GetOrigin(),
				owner.GetAngles()));

			worldEditorApi.ModifyEntityKey(nameProxy, "m_rTarget", EL_Utils.GetPrefabName(owner));
			worldEditorApi.EndEntityAction();
		}

		worldEditorApi.SetEntitySelection(nameProxy);
		worldEditorApi.UpdateSelectionGui();
	}

	//------------------------------------------------------------------------------------------------
	protected string GenerateName(IEntity owner)
	{
		string prefabNameOnly = FilePath.StripExtension(FilePath.StripPath(EL_Utils.GetPrefabName(owner)));
		if (!prefabNameOnly)
			prefabNameOnly = owner.ClassName();

		return string.Format("%1_%2", prefabNameOnly, Workbench.GenerateGloballyUniqueID64());
	}
	#endif
};
