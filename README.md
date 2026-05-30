# Squad Coordination AI in Unreal Engine

## Research Question

How can formation movement be implemented for a squad of AI agents in Unreal Engine while maintaining believable coordination and adaptability in dynamic environments?

## Abstract

In many games, AI agents are often controlled as individual characters. However, squad-based enemies or companions need to move as a coordinated group to appear believable to the player. This project investigates how formation movement can be implemented in Unreal Engine by combining squad-level coordination with individual agent navigation.

The implemented prototype uses role-based formation slots, behavior tree states, NavMesh target projection, local avoidance, stuck recovery, and automatic formation adaptation. A comparison mode is included where every agent moves toward the same shared target. This baseline makes it possible to compare coordinated formation movement against a simpler approach where agents do not receive individual formation slots.

The goal is not to create perfectly rigid military movement, but to create group movement that remains readable, believable, and adaptable when the squad moves through different spaces.

## Introduction

Squad movement is an important part of believable game AI. When several AI agents move together, they should appear aware of each other and of the surrounding environment. If every agent simply receives the same destination, the group can overlap, cluster, or move in a chaotic way. If the formation is too strict, the squad can look robotic and fail to adapt to obstacles or narrow spaces.

This research focuses on balancing centralized coordination with local adaptation. The squad-level system defines the desired formation, roles, and target slots. Individual agents then use Unreal Engine navigation and steering behavior to move toward those slots while still reacting to local movement problems.

## Background

### Formation Movement

Formation movement gives each agent a target position inside a group shape instead of sending all agents to the same destination. Common formation examples are wedge, column, and line. This makes the squad more readable because every member has a clear place in the group.

### Leader-Follower Coordination

The prototype uses a shared squad target as the center of the formation. Each agent calculates its slot as an offset from this target. The formation orientation is based on the leader's rotation, allowing the formation to rotate as the squad moves.

### Role-Based Positioning

Each squad member has a role: leader, left flank, right flank, or rear support. These roles determine the agent's default position in the wedge formation. The role system makes the formation more meaningful than a purely visual pattern, because agents can be assigned different behavior or tactical meaning.

### Believable Adaptation

Believable squad coordination does not require agents to hold perfect positions at all times. In dynamic environments, agents may need to avoid each other, temporarily relax their assigned slot, or switch formation when the level layout becomes narrow. The prototype supports this by combining NavMesh projection, local avoidance, stuck detection, relaxed fallback slots, and automatic formation switching.

## Method

The prototype compares two squad movement modes.

| Mode | Description |
| --- | --- |
| Shared target baseline | Every squad member receives the same target position. |
| Role-based formation | Every squad member receives an individual formation slot. |

The baseline is included to show why squad coordination is useful. In the baseline mode, agents can reach the target but tend to cluster around the same point. In the role-based mode, the squad keeps a more readable shape because each agent has its own assigned location.

## Implementation

The main implementation is located in `Source/GameAIProg/SquadCoordination/Level_SquadCoordination.cpp`.

The system includes:

- Role-based squad members: leader, left flank, right flank, and rear support.
- Formation layouts: wedge, column, and line.
- A shared squad target set by mouse input.
- Individual formation slots calculated from the squad target and active formation.
- NavMesh projection for formation slots.
- A shared-target baseline comparison mode.
- Automatic formation adaptation based on nearby navigable space.
- Local avoidance through an avoidance-aware arrive steering behavior.
- Behavior tree states for formation following, rejoining, low-health fallback, ally support, and patrol behavior.
- A patrol enemy used to test low-health fallback and ally support.
- Runtime metrics shown in the ImGui panel.

### Formation Slots

Formation slots are stored as local offsets from the squad target. For example, the wedge formation places the leader at the front, flankers behind and to the sides, and support behind the group.

```text
Wedge formation

          Leader
        /        \
 Left Flank    Right Flank
        \        /
        Rear Support
```

Example offsets:

| Role | Local Offset |
| --- | --- |
| Leader | `(0, 0)` |
| Left Flank | `(-spacing, -spacing)` |
| Right Flank | `(-spacing, spacing)` |
| Rear Support | `(-2 * spacing, 0)` |

The local offset is rotated using the leader's direction and then added to the squad target. This produces a world-space slot for each agent.

### Navigation

Each desired slot is projected onto Unreal Engine's NavMesh before it is used as a movement target. If a slot cannot be projected, the system falls back to the squad center or the agent's current navigable area. This prevents agents from receiving unreachable targets outside the navigation mesh.

### Behavior Tree States

The squad uses behavior tree logic to select the current movement state for each agent.

| State | Purpose |
| --- | --- |
| Follow Formation | Move toward the assigned formation slot. |
| Rejoin Squad | Return to formation after drifting too far or using a relaxed slot. |
| Low Health Fallback | Move a wounded agent away from danger. |
| Support Low Health Ally | Hold position and face the enemy while an ally is unsafe. |
| Patrol | Used by the patrol enemy test actor. |

![Support behavior when an enemy threatens a low-health teammate](Content/Gifs/enemy_defence.gif)

*Figure 1: A healthy squad member supports a low-health teammate while the patrol enemy is in range.*

### Stuck Recovery

The prototype detects a stuck agent by checking whether the agent is still far from its assigned slot while making very little movement progress. When this happens, the agent temporarily receives a relaxed slot near its original formation position. After reaching that relaxed slot or making progress again, the agent returns to its normal formation slot.

This keeps the formation from becoming completely rigid while still preserving the role-based offset as much as possible.

### Automatic Formation Adaptation

The `Automatic formation` option in the ImGui panel lets the squad choose its formation based on nearby navigable space. The system samples the NavMesh around the squad target once when a new target is selected, then keeps that formation until the player clicks a new target or changes the formation settings.

| Environment check | Chosen formation |
| --- | --- |
| Both sides are open | Wedge |
| Left or right side is blocked | Column |
| Sides are open but forward space is blocked | Line |

When automatic formation is disabled, the formation can be selected manually from the ImGui combo box.

![Automatic formation adaptation while the squad moves through changing terrain](Content/Gifs/automatic_formation.gif)

*Figure 2: The squad automatically changes formation when the surrounding navigable space changes.*

## Evaluation

The evaluation uses the same movement scenarios in both baseline mode and role-based formation mode.

| Scenario | What is tested |
| --- | --- |
| Open area movement | Whether the squad keeps a readable shape in free space. |
| Narrow corridor | Whether automatic adaptation switches to column. |
| Obstacle in front | Whether automatic adaptation can switch to line. |
| Static obstacle path | Whether agents avoid clustering and recover from disruption. |
| Low-health ally near enemy | Whether fallback and support states activate. |

The ImGui panel reports these runtime metrics:

| Metric | Meaning |
| --- | --- |
| Average slot error | Average distance between each agent and its assigned target slot. |
| Max slot error | Worst current distance between an agent and its assigned slot. |
| Average spacing error | Difference between current agent spacing and desired formation spacing. |
| Stuck agents | Number of agents detected as not making enough movement progress. |
| Relaxed slots | Number of agents temporarily using a fallback slot. |
| Settle time | Time needed for the formation to become close enough to its target slots. |

These values make the comparison more concrete than judging the movement only by eye.

## Discussion

The role-based formation mode gives the squad a more readable structure than the shared-target baseline. Because every agent receives a different slot, the group is less likely to collapse into a single cluster around the destination. The use of NavMesh projection also helps keep assigned slots valid inside the level.

The automatic formation system improves adaptability by changing the active formation when the terrain changes. A wedge is useful in open areas because it spreads the squad out. A column is better in corridors because it reduces side-by-side spacing. A line is useful when the front is blocked but side space is available.

The stuck recovery system helps prevent agents from permanently failing when their exact slot is difficult to reach. Instead of forcing the exact slot forever, the agent can temporarily use a nearby relaxed slot and then return to formation.

## Limitations

- The squad manager logic currently lives in the level script instead of a reusable squad manager actor or component.
- Automatic formation adaptation is rule-based and only samples a few NavMesh points around the squad target.
- Stuck recovery is based on movement progress, not on full path reasoning.
- The enemy is used as a test stimulus for support and fallback behavior, not as a complete combat system.
- Roles mainly affect formation placement and simple support/fallback behavior. They do not yet include deeper tactical actions such as suppressing, cover usage, or coordinated flanking attacks.

## Conclusion

Formation movement for a squad of AI agents can be implemented in Unreal Engine by combining centralized squad coordination with individual navigation. The prototype shows that assigning role-based formation slots creates more readable group movement than sending every agent to the same target.

Believable coordination comes from allowing the squad to adapt instead of forcing a perfect shape at all times. NavMesh projection keeps slots valid, local avoidance reduces overlap, stuck recovery helps agents rejoin after disruption, and automatic formation switching allows the squad to respond to open areas, corridors, and blocked forward space.

The result is a squad system that is structured enough to look coordinated, but flexible enough to remain believable in a dynamic level.

## References

- Craig Reynolds, steering behaviors for autonomous characters.
- Ian Millington and John Funge, *Artificial Intelligence for Games*.
- Mat Buckland, *Programming Game AI by Example*.
- Unreal Engine documentation for AIController, Behavior Trees, Blackboards, and Navigation Mesh.
