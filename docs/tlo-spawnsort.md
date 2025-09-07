---
tags:
  - tlo
---
# `SpawnSort`

<!--tlo-desc-start-->
Returns the nth spawn that matches your [spawn search] string, sorting in ascending or descending order by any spawn member.

There's a special case to handle MQ2Nav as well, see examples.
<!--tlo-desc-end-->

## Forms
<!--tlo-forms-start-->
### {{ renderMember(type='spawn', name='SpawnSort', params='<n>,<asc|desc>,<member>,<spawnsearch>') }}

:   Returns the nth spawn that matches your [spawn search] string, sorting in ascending or descending order by whatever member you like (Level, PctHPs, etc). The member to sort by can be anything on a spawn type, so if you can do `${Spawn[something].<member>}`, it'll work. A notable exception is "sub-members", like `Class.ShortName`. If there's a period in there, it won't work 😔.

### {{ renderMember(type='xtarget', name='SpawnSort', params='<n>,<asc|desc>,<member>,<xtarget-specific spawnsearch>') }}

:   If the [spawn search] string restricts it to NPCs on your xtarget (i.e. includes xtarhater), you can sort by xtarget members, e.g. ${SpawnSort[1,asc,PctAggro,xtarhater].PctAggro, and it will return XTargetType instead of SpawnType. 

### {{ renderMember(type='groupmember', name='SpawnSort', params='<n>,<asc|desc>,<member>,group') }}

:   You can also use "group" in [spawn search], and sort by something on groupmember (probably the most useful is PctAggro again). It'll return GroupMemberType too. 
<!--tlo-forms-end-->

## Examples
<!--tlo-examples-start-->
A list of spawn types you can search can be found on [Spawn search](../macroquest/reference/general/spawn-search.md).

Some random examples:

The lowest level player within 100 yards
:   `${SpawnSort[1,asc,Level,pc radius 100]}`

The 2nd highest level npc in the zone
:   `${SpawnSort[2,desc,Level,npc]}`

Lowest HP group member
:   `${SpawnSort[1,asc,PctHPs,group]}`

It handles a a special case for MQ2Nav as well, you can do:
:   `/varset closestNPCID ${SpawnSort[1,asc,PathLength,npc radius 1000].ID}`

Which will give you the closest NPC within 1000 yards, but using MQ2Nav path length instead of just straight line distance. PathExists works too, in case you just want to find the first spawn you can navigate to but don't care about distance.

If the spawn search string restricts it to NPCs on your xtarget (i.e. includes xtarhater), you can sort by xtarget members, e.g. PctAggro, and it will return XTargetType instead of SpawnType. E.g.
```bash
# if any npcs on xtarget have less than 100% aggro
/if (${SpawnSort[1,asc,PctAggro,xtarhater].PctAggro} < 100) {
    /varset npcIDontHaveAggroOnID ${SpawnSort[1,asc,PctAggro,xtarhater].ID}
    # stuff to get aggro back
}
```

You can also use "group" in [spawn search], and sort by something on groupmember (probably the most useful is PctAggro again). It'll return GroupMemberType too. e.g.
```bash
# if the highest aggro player in the group isn't the main assist
/if (!${SpawnSort[1,desc,PctAggro,group].MainAssist}) {
    /varset playerWithAggroID ${SpawnSort[1,desc,PctAggro,group].ID}
    # heal them or whatever
}
```

<!--tlo-examples-end-->

<!--tlo-linkrefs-start-->
[groupmember]: ../macroquest/reference/data-types/datatype-groupmember.md
[spawn]: ../macroquest/reference/data-types/datatype-spawn.md
[xtarget]: ../macroquest/reference/data-types/datatype-xtarget.md
[spawn search]: ../macroquest/reference/general/spawn-search.md
<!--tlo-linkrefs-end-->