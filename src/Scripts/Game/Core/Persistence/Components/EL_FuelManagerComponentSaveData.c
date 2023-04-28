[EL_ComponentSaveDataType(EL_FuelManagerComponentSaveDataClass, FuelManagerComponent), BaseContainerProps()]
class EL_FuelManagerComponentSaveDataClass : EL_ComponentSaveDataClass
{
};

[EL_DbName(EL_FuelManagerComponentSaveData, "FuelManager")]
class EL_FuelManagerComponentSaveData : EL_ComponentSaveData
{
	ref array<ref EL_PersistentFuelNode> m_aFuelNodes;

	//------------------------------------------------------------------------------------------------
	override EL_EReadResult ReadFrom(notnull GenericComponent worldEntityComponent, notnull EL_ComponentSaveDataClass attributes)
	{
		m_aFuelNodes = new array<ref EL_PersistentFuelNode>();

		array<BaseFuelNode> outNodes();
		FuelManagerComponent.Cast(worldEntityComponent).GetFuelNodesList(outNodes);

		foreach (BaseFuelNode node : outNodes)
		{
			SCR_FuelNode fuelNode = SCR_FuelNode.Cast(node);
			if (!fuelNode)
			{
				Debug.Error(string.Format("'%1' contains non persistable fuel node type '%2'. Inherit from SCR_FuelNode instead. Ignored.", worldEntityComponent, node.Type()));
				continue;
			}

			EL_PersistentFuelNode persistentFuelNode();
			persistentFuelNode.m_iTankId = fuelNode.GetFuelTankID();
			persistentFuelNode.m_fFuel = fuelNode.GetFuel();

			if (attributes.m_bTrimDefaults)
			{
				if (persistentFuelNode.m_fFuel >= fuelNode.GetMaxFuel()) continue;

				float initalFuelState;
				if (EL_ReflectionUtils<float>.Get(fuelNode, "m_fInitialFuelTankState", initalFuelState) &&
					float.AlmostEqual(persistentFuelNode.m_fFuel, initalFuelState)) continue;
			}

			m_aFuelNodes.Insert(persistentFuelNode);
		}

		if (m_aFuelNodes.IsEmpty()) return EL_EReadResult.DEFAULT;
		return EL_EReadResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override bool ApplyTo(notnull GenericComponent worldEntityComponent, notnull EL_ComponentSaveDataClass attributes)
	{
		array<BaseFuelNode> outNodes();
		FuelManagerComponent.Cast(worldEntityComponent).GetFuelNodesList(outNodes);

		bool tryIdxAcces = outNodes.Count() >= m_aFuelNodes.Count();

		foreach (int idx, EL_PersistentFuelNode persistentFuelNode : m_aFuelNodes)
		{
			SCR_FuelNode fuelNode;

			// Assume same ordering as on save and see if that matches
			if (tryIdxAcces)
			{
				SCR_FuelNode idxNode = SCR_FuelNode.Cast(outNodes.Get(idx));

				if (idxNode.GetFuelTankID() == persistentFuelNode.m_iTankId) fuelNode = idxNode;
			}

			// Iterate all fuel nodes to hopefully find the right tank id
			if (!fuelNode)
			{
				foreach (BaseFuelNode findNode : outNodes)
				{
					SCR_FuelNode findFuelNode = SCR_FuelNode.Cast(findNode);
					if (findFuelNode && findFuelNode.GetFuelTankID() == persistentFuelNode.m_iTankId)
					{
						fuelNode = findFuelNode;
						break;
					}
				}
			}

			if (!fuelNode)
			{
				Debug.Error(string.Format("'%1' unable to find fuel tank id '%2'. Ignored.", worldEntityComponent, persistentFuelNode.m_iTankId));
				continue;
			}

			fuelNode.SetFuel(persistentFuelNode.m_fFuel);
		}

		return true;
	}
};

class EL_PersistentFuelNode
{
	int m_iTankId;
	float m_fFuel;
};
