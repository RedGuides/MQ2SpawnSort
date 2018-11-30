#include "../MQ2Plugin.h"
#include <functional>

PLUGIN_VERSION(0.2);
PreSetup("MQ2SpawnSort");

BOOL dataSpawnSort(PCHAR szIndex, MQ2TYPEVAR &Ret);

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

BOOL dataSpawnSort(PCHAR szIndex, MQ2TYPEVAR &Ret)
{
	// szIndex format:
	// <n>,<asc|desc>,<member>,<spawn search string>
	char szArg[MAX_STRING] = { 0 };

	unsigned int n;
	bool ascending;
	char member[MAX_STRING] = { 0 };
	char index[MAX_STRING] = { 0 };
	SEARCHSPAWN searchSpawn;

	// n
	if (GetArg(szArg, szIndex, 1, FALSE, FALSE, TRUE) && strlen(szArg) > 0)
	{
		char * pFound;
		n = strtoul(szArg, &pFound, 10);
		if (!pFound || n < 1)
			return false;
	}
	else
		return false;

	// asc|desc
	if (GetArg(szArg, szIndex, 2, FALSE, FALSE, TRUE) && strlen(szArg) > 0)
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
	if (!GetArg(member, szIndex, 3, FALSE, FALSE, TRUE) || strlen(szArg) == 0)
		return false;

	// spawn search string, it's whatever's left
	strcpy_s(szArg, GetNextArg(szIndex, 3, TRUE));
	if (strlen(szArg) > 0)
	{
		ClearSearchSpawn(&searchSpawn);
		ParseSearchSpawn(szArg, &searchSpawn);
	}
	else
		return false;

	// Sorted container for results. Only one of these will be used, which one will depend on the type of the first result
	std::multimap<double, PSPAWNINFO> listByDouble;
	std::multimap<__int64, PSPAWNINFO> listBySigned;
	std::multimap<std::string, PSPAWNINFO> listByString;
	// This should probably just be a key of MQ2TYPEVAR with a comparator created for it but that'll cause problems with string storage so meh

	PSPAWNINFO pOrigin = searchSpawn.FromSpawnID ? (PSPAWNINFO)GetSpawnByID(searchSpawn.FromSpawnID) : (PSPAWNINFO)pCharSpawn; // idk what this is but superwho does it so I will too
	PSPAWNINFO pSpawn = (PSPAWNINFO)pSpawnList;

	// Iterate through spawn list, get desired member for each spawn, and add it to a map
	while (pSpawn)
	{
		if (SpawnMatchesSearch(&searchSpawn, pOrigin, pSpawn))
		{
			MQ2TYPEVAR typeVar = { 0 };
			MQ2TYPEVAR ret = { 0 };

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
				for (auto i = 0; i < 13; i++)
				{
					if (GetCharInfo()->pXTargetMgr->XTargetSlots[i].SpawnID == pSpawn->SpawnID)
					{
						typeVar.Type = pXTargetType;
						typeVar.DWord = i;
						break;
					}
				}
			}
			else if (searchSpawn.bGroup)
			{
				for (auto i = 0; i < 6; i++)
				{
					if (!GetCharInfo()->pGroupInfo->pMember[i])
						continue;

					if (GetCharInfo()->pGroupInfo->pMember[i]->pSpawn == pSpawn)
					{
						typeVar.Type = pGroupMemberType;
						typeVar.DWord = i;
						break;
					}
				}
			}
			// Default to plain old spawn
			else
			{
				typeVar.Type = pSpawnType;
				typeVar.Ptr = pSpawn;
			}

			// MQ2Nav outputs a message when it fails to find a path, it'll get a bit spammy if we don't filter
			BOOL Temp = gFilterMQ;
			gFilterMQ = true;

			if (!typeVar.Type->GetMember(typeVar.VarPtr, member, index, ret))
			{
				DebugSpew("SpawnSort - GetMember failed");
			}
			// Sanity check on PathLength results, it returns -1 if no path found in which case we should ignore this result
			// If we're not using PathLength, or we are but result isn't -1, use function determined above to add to a list
			else if (_stricmp(member, "PathLength") || ret.Float != -1.0f)
			{
				// Figure out which function to use to add to a map, based type of result, if we don't already know it
				if (ret.Type == pDoubleType)
					listByDouble.insert(std::pair<double, PSPAWNINFO>(ret.Double, pSpawn));
				else if (ret.Type == pFloatType)
					listByDouble.insert(std::pair<double, PSPAWNINFO>(ret.Float, pSpawn));
				else if (ret.Type == pInt64Type)
					listBySigned.insert(std::pair<__int64, PSPAWNINFO>(ret.Int64, pSpawn));
				else if (ret.Type == pIntType || ret.Type == pByteType || ret.Type == pBoolType)
					listBySigned.insert(std::pair<__int64, PSPAWNINFO>(ret.Int, pSpawn));
				else
				{
					// Default, calls ToString
					char buffer[MAX_STRING] = { 0 };
					typeVar.Type->ToString(typeVar.VarPtr, buffer);
					listByString.insert(std::pair<std::string, PSPAWNINFO>(std::string(buffer), pSpawn));
				};
			}

			gFilterMQ = false;
		}

		pSpawn = pSpawn->pNext;
	}

	// Now one of the maps contains a list of spawns that match the search string, sorted by whatever member
	// Iterators are confusing me atm so copying to a vector and accessing by index is way easier
	std::vector<PSPAWNINFO> results;

	if (listByDouble.size() > 0)
		for each (auto tuple in listByDouble)
			results.push_back(tuple.second);
	else if (listBySigned.size() > 0)
		for each (auto tuple in listBySigned)
			results.push_back(tuple.second);
	else if (listByString.size() > 0)
		for each (auto tuple in listByString)
			results.push_back(tuple.second);
	else
		return false;

	if (n > results.size())
		return false;

	pSpawn = results[ascending ? n - 1 : results.size() - n];
	
	// Return the "best" possible type based on the spawn search, defaulting to Spawn
	Ret.Type = pSpawnType;
	Ret.Ptr = pSpawn;

	if (searchSpawn.bXTarHater)
	{
		for (auto i = 0; i < 13; i++)
		{
			if (GetCharInfo()->pXTargetMgr->XTargetSlots[i].SpawnID == pSpawn->SpawnID)
			{
				Ret.Type = pXTargetType;
				Ret.DWord = i;
				break;
			}
		}
	}
	else if (searchSpawn.bGroup)
	{
		for (auto i = 0; i < 6; i++)
		{
			if (!GetCharInfo()->pGroupInfo->pMember[i])
				continue;

			if (GetCharInfo()->pGroupInfo->pMember[i]->pSpawn == pSpawn)
			{
				Ret.Type = pGroupMemberType;
				Ret.DWord = i;
				break;
			}
		}
	}

	return true;
}