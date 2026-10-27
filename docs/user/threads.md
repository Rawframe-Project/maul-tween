# Threads and capacity

The library starts no thread. A context and everything in it is used by one thread at a time; the values layer (easings, springs, interpolation, paths, timing, staggers) and a finished clip's tracks may be used from any thread. A context can hand its clips' players to the host's task system, and it grows only when the host asks.

## Clips' players on the host's tasks

The host gives the context def two functions: one that hands out a range of work items, run on any thread and in any order, and one that waits for them. This is the same pair Maul VFX takes, so one task system serves both.

```c
static void* RunNow(mtwnTaskFn* task, int32_t itemCount, int32_t minRange, void* taskContext,
                    void* userContext)
{
    (void)minRange;
    (void)userContext;
    task(0, itemCount, taskContext); // a real host splits it over its workers
    return NULL;                     // ran it all: nothing to wait for
}

static void WaitFor(void* userTask, void* userContext)
{
    (void)userTask;
    (void)userContext;
}
```

```c
mtwnContextDef threaded = mtwnDefaultContextDef();
threaded.enqueueTask = RunNow;
threaded.finishTask = WaitFor;
threaded.userTaskContext = NULL; // passed to both, the host's own
mtwnContext* parallel = NULL;
mtwnResult madeThreaded = mtwnCreateContext(&threaded, &parallel);
```

Each update then hands the host up to 16 groups of the clips' players, in the order they were made. A group advances its players in scratch of its own; the update then composes what they made into the targets in creation order, between the transitions. Records, values, velocities and the changed list are the same bits as without the task system, whatever the split and the order.

The groups cost memory, about 900 KB more for the default def (`mtwnContextDef` gives the sizes), so a host with a few players leaves the functions unset.

## Players that stay on the calling thread

Transitions, and targets several players write, are composed on the calling thread. Some players are advanced there too:

- a player whose clip has more than 128 nodes while other players run, since a group keeps times for 128 nodes;
- a player whose records do not fit its group's staging (1,024 records, or the queue's capacity when that is smaller), with the players after it in its group;
- a player with targets of its own whose results might not fit its group's staging, which a group cannot undo once written.

`mtwnContext_GetSerialAdvances` counts these advances since the context was made. When it grows, larger binding or event capacities, or smaller clips, keep the players in their groups.

## Players in step

Players of one clip at the same time and rate, such as a crowd started together, share their sample and their records: the second and later ones take the work the first did. The sharing holds `sharedNodeCapacity` nodes and `sharedRecordCapacity` records on the calling thread and in each group, 14,848 bytes each at the defaults on a 64-bit target. A host whose players never run in step sets both to 0 and holds none of it.

## Growing

Capacities are set when the context is made. When the host outgrows them, it grows the context, once, at a moment it chooses:

```c
mtwnContextDef bigger = def;
bigger.targetCapacity *= 2;
bigger.playerCapacity *= 2;
mtwnResult grown = mtwnGrowContext(&context, &bigger); // it may move; ids stay valid
```

Each capacity must be at least the context's own, the sharing's excepted, and the allocator the same. Every id stays valid and everything playing plays on. The context moves to one new block, so the call writes its new address back; clips' storage does not move. On a refusal or a failed allocation nothing changes. `mtwnGrowContext` can also give a context a task system it was made without.
