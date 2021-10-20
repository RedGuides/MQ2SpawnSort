#include <mq/Plugin.h>
#include <functional>

PLUGIN_VERSION(0.2);
PreSetup("MQ2SpawnSort");

bool dataSpawnSort(const char* szIndex, MQTypeVar& Ret);

// Called once, when the plugin is to initialize
PLUGIN_API VOID InitializePlugin(VOID)
{
	AddMQ2Data("SpawnSort", dataSpawnSort);
}

// Called once, when the plugin is to shutdown
PLUGIN_API VOID ShutdownPlugin(VOID)
{
	RemoveMQ2Data("SpawnSort");
}

bool dataSpawnSort(const char* szIndex, MQTypeVar &Ret)
{
	// szIndex format:
	// <n>,<asc|desc>,<member>,<spawn search string>
	char szArg[MAX_STRING] = { 0 };

	char mutableIndex[MAX_STRING] = { 0 };
	strcpy_s(mutableIndex, szIndex);

	unsigned int n;
	bool ascending;
	char member[MAX_STRING] = { 0 };
	char index[MAX_STRING] = { 0 };
	MQSpawnSearch searchSpawn;

	// n
	if (GetArg(szArg, mutableIndex, 1, FALSE, FALSE, TRUE) && strlen(szArg) > 0)
	{
		char * pFound;
		n = strtoul(szArg, &pFound, 10);
		if (!pFound || n < 1)
			return false;
	}
	else
		return false;

	// asc|desc
	if (GetArg(szArg, mutableIndex, 2, FALSE, FALSE, TRUE) && strlen(szArg) > 0)
	{
		if (!_stricmp(szArg, "asc"))
			ascending = true;
		else if (!_stricmp(szArg, "desc"))
			ascending = false;
		else
			return false;
	}
	else
		return false;

	// member
	if (!GetArg(member, mutableIndex, 3, FALSE, FALSE, TRUE) || strlen(szArg) == 0)
		return false;

	// spawn search string, it's whatever's left
	strcpy_s(szArg, GetNextArg(mutableIndex, 3, TRUE));
	if (strlen(szArg) > 0)
	{
		ClearSearchSpawn(&searchSpawn);
		ParseSearchSpawn(szArg, &searchSpawn);
	}
	else
		return false;

	// Sorted container for results. Only one of these will be used, which one will depend on the type of the first result
	std::multimap<double, PSPAWNINFO> listByDouble;
	std::multimap<int64_t, PSPAWNINFO> listBySigned;
	std::multimap<std::string, PSPAWNINFO> listByString;
	// This should probably just be a key of MQTypeVar with a comparator created for it but that'll cause problems with string storage so meh

	PSPAWNINFO pOrigin = searchSpawn.FromSpawnID ? GetSpawnByID(searchSpawn.FromSpawnID) : pLocalPlayer;
	PSPAWNINFO pSpawn = (PSPAWNINFO)pSpawnList;

	// Iterate through spawn list, get desired member for each spawn, and add it to a map
	while (pSpawn)
	{
		if (SpawnMatchesSearch(&searchSpawn, pOrigin, pSpawn))
		{
			MQTypeVar typeVar;

			// Special cases for MQ2Nav members
			if (!_stricmp(member, "PathExists") || !_stricmp(member, "PathLength"))
			{
				typeVar.Type = FindMQ2DataType("Navigation");
				if (!typeVar.Type)
					return false;

				sprintf_s(index, "id %d", pSpawn->SpawnID);
			}

			// If search string allows us to restrict to group member or xtarget, we can use those types instead of spawn
			else if (searchSpawn.bXTarHater)
			{
				for (auto i = 0; i < pLocalPC->pExtendedTargetList->GetNumSlots(); i++)
				{
					if (pLocalPC->pExtendedTargetList->GetSlot(i)->SpawnID == pSpawn->SpawnID)
					{
						typeVar.Type = mq::datatypes::pXTargetType;
						typeVar.DWord = i;
						break;
					}
				}
			}
			else if (searchSpawn.bGroup)
			{
				for (auto i = 0; i < MAX_GROUP_SIZE; i++)
				{
					SPAWNINFO* groupMember = GetGroupMember(i);
					if (groupMember && groupMember->SpawnID == pSpawn->SpawnID)
					{
						typeVar.Type = mq::datatypes::pGroupMemberType;
						typeVar.DWord = i;
						break;
					}
				}
			}
			// Default to plain old spawn
			else
			{
				typeVar = mq::datatypes::pSpawnType->MakeTypeVar(pSpawn);
			}

			// MQ2Nav outputs a message when it fails to find a path, it'll get a bit spammy if we don't filter
			bool Temp = gFilterMQ;
			gFilterMQ = true;
			MQTypeVar ret;

			if (!typeVar.Type || !typeVar.Type->GetMember(typeVar.VarPtr, member, index, ret))
			{
				DebugSpew("SpawnSort - GetMember failed");
			}

			// Sanity check on PathLength results, it returns -1 if no path found in which case we should ignore this result
			// If we're not using PathLength, or we are but result isn't -1, use function determined above to add to a list
			else if (_stricmp(member, "PathLength") || ret.Float != -1.0f)
			{
				// Figure out which function to use to add to a map, based type of result, if we don't already know it
				if (ret.Type == mq::datatypes::pDoubleType)
					listByDouble.emplace(ret.Double, pSpawn);
				else if (ret.Type == mq::datatypes::pFloatType)
					listByDouble.emplace(ret.Float, pSpawn);
				else if (ret.Type == mq::datatypes::pInt64Type)
					listBySigned.emplace(ret.Int64, pSpawn);
				else if (ret.Type == mq::datatypes::pIntType || ret.Type == mq::datatypes::pByteType || ret.Type == mq::datatypes::pBoolType)
					listBySigned.emplace(ret.Int, pSpawn);
				else
				{
					// Default, calls ToString
					char buffer[MAX_STRING] = { 0 };
					typeVar.Type->ToString(typeVar.VarPtr, buffer);

					listByString.emplace(buffer, pSpawn);
				};
			}

			gFilterMQ = false;
		}

		pSpawn = pSpawn->GetNext();
	}

	// Now one of the maps contains a list of spawns that match the search string, sorted by whatever member
	// Iterators are confusing me atm so copying to a vector and accessing by index is way easier
	std::vector<PSPAWNINFO> results;

	if (!listByDouble.empty())
	{
		for (const auto& tuple : listByDouble)
			results.push_back(tuple.second);
	}
	else if (!listBySigned.empty())
	{
		for (const auto& tuple : listBySigned)
			results.push_back(tuple.second);
	}
	else if (listByString.size() > 0)
	{
		for (const auto& tuple : listByString)
			results.push_back(tuple.second);
	}
	else
	{
		return false;
	}

	if (n > results.size())
		return false;

	pSpawn = results[ascending ? n - 1 : results.size() - n];

	if (searchSpawn.bXTarHater)
	{
		for (auto i = 0; i < pLocalPC->pExtendedTargetList->GetNumSlots(); i++)
		{
			if (pLocalPC->pExtendedTargetList->GetSlot(i)->SpawnID == pSpawn->SpawnID)
			{
				Ret.Type = mq::datatypes::pXTargetType;
				Ret.DWord = i;
				return true;
			}
		}
	}
	else if (searchSpawn.bGroup)
	{
		for (auto i = 0; i < MAX_GROUP_SIZE; i++)
		{
			SPAWNINFO* groupMember = GetGroupMember(i);
			if (groupMember && groupMember->SpawnID == pSpawn->SpawnID)
			{
				Ret.Type = mq::datatypes::pGroupMemberType;
				Ret.DWord = i;
				break;
			}
		}
	}

	// Return the "best" possible type based on the spawn search, defaulting to Spawn
	Ret = mq::datatypes::pSpawnType->MakeTypeVar(pSpawn);

	return true;
}
