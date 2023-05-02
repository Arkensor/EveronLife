[BaseContainerProps()]
class EL_BaseMuzzleComponentSaveDataClass : EL_ComponentSaveDataClass
{
	//------------------------------------------------------------------------------------------------
	override array<typename> Requires()
	{
		// Make sure attached magazines are alraedy loaded
		return {EL_WeaponAttachmentsStorageComponentSaveDataClass};
	}
};

class EL_BaseMuzzleComponentSaveData : EL_ComponentSaveData
{
	EMuzzleType m_eMuzzleType;
	ref array<bool> m_aChamberStatus;

	//------------------------------------------------------------------------------------------------
	override EL_EReadResult ReadFrom(notnull GenericComponent worldEntityComponent, notnull EL_ComponentSaveDataClass attributes)
	{
		BaseMuzzleComponent muzzle = BaseMuzzleComponent.Cast(worldEntityComponent);
		int barrelsCount = muzzle.GetBarrelsCount();
		int ammoCount = muzzle.GetAmmoCount();

		m_eMuzzleType = muzzle.GetMuzzleType();
		m_aChamberStatus = {};
		m_aChamberStatus.Reserve(barrelsCount);

		for (int nBarrel = 0; nBarrel < barrelsCount; nBarrel++)
		{
			m_aChamberStatus.Insert(muzzle.IsBarrelChambered(nBarrel) || nBarrel < ammoCount);
		}

		return EL_EReadResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override EL_EApplyResult ApplyTo(notnull GenericComponent worldEntityComponent, notnull EL_ComponentSaveDataClass attributes)
	{
		BaseMuzzleComponent muzzle = BaseMuzzleComponent.Cast(worldEntityComponent);

		foreach (int idx, bool chambered : m_aChamberStatus)
		{
			if (!chambered)
				muzzle.ClearChamber(idx);
		}

		return EL_EApplyResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override bool IsFor(notnull GenericComponent worldEntityComponent, notnull EL_ComponentSaveDataClass attributes)
	{
		BaseMuzzleComponent muzzle = BaseMuzzleComponent.Cast(worldEntityComponent);
		return muzzle.GetMuzzleType() == m_eMuzzleType && muzzle.GetBarrelsCount() == m_aChamberStatus.Count();
	}

	//------------------------------------------------------------------------------------------------
	override bool Equals(notnull EL_ComponentSaveData other)
	{
		EL_BaseMuzzleComponentSaveData otherData = EL_BaseMuzzleComponentSaveData.Cast(other);

		if (m_eMuzzleType != otherData.m_eMuzzleType ||
			m_aChamberStatus.Count() != otherData.m_aChamberStatus.Count())
			return false;

		foreach (int idx, bool chambered : m_aChamberStatus)
		{
			if (chambered != m_aChamberStatus.Get(idx))
				return false;
		}

		return true;
	}
};