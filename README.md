# Squad Coordination AI in Unreal Engine

## Research Question

How can formation movement be implemented for a squad of AI agents in Unreal Engine while maintaining believable coordination and adaptability in dynamic environments?

## Portfolio Piece Description

This project researches and prototypes a squad coordination AI system in Unreal Engine. The system focuses on dynamic formation movement, role-based positioning, obstacle adaptation, and believable group behavior for AI-controlled agents.

The prototype also includes a research comparison mode. The squad can be switched between a coordinated role-based formation and a simple shared-target baseline where every agent receives the same destination. This makes it possible to compare coordinated movement against a naive group movement approach inside the same level.

## Abstract

In many games, AI agents are often controlled as individual characters. However, squad-based enemies or companions need to move as a coordinated group to appear believable to the player. This research investigates how formation movement can be implemented in Unreal Engine by combining a squad-level coordination system with individual AI navigation.

The proposed system uses a squad manager to assign agents to formation slots based on their role, such as leader, flanker, support, or rear guard. Each slot is calculated relative to the squad leader or formation center. Unreal Engine's navigation system is then used to move each agent toward its assigned position. To improve adaptability, the system should allow agents to temporarily break formation when blocked by obstacles and return to their assigned slot when the path becomes available again.

The goal of this research is not to create perfectly rigid formation movement, but to create movement that feels coordinated, readable, and believable during gameplay.

## Introduction

Squad movement is an important part of believable game AI. When multiple AI agents move together, they should appear aware of each other and of the environment around them. If every agent simply moves independently toward the same target, the group can look chaotic, overlap with each other, or get stuck around obstacles. On the other hand, if the formation is too strict, the agents can look robotic and fail to adapt naturally to the level geometry.

This research focuses on formation movement for a squad of AI agents in Unreal Engine. The main challenge is finding a balance between centralized coordination and local adaptation. A squad-level system can provide structure by assigning positions and roles, while each individual AI agent still needs enough freedom to navigate around obstacles and react to movement problems.

## Research Goals

- Investigate how squad formations can be represented in Unreal Engine.
- Design a role-based formation system for multiple AI agents.
- Explore how agents can maintain formation while moving through an environment.
- Research how obstacles and narrow spaces can affect formation movement.
- Create a believable balance between group coordination and individual navigation.
- Evaluate whether the resulting movement appears clear and natural to the player.

## Background

### Formation Movement

Formation movement is a technique where multiple agents move together while trying to maintain a specific shape. Common examples include line, wedge, column, and diamond formations. Instead of giving every agent the same destination, each agent receives a target position inside the formation.

### Leader-Follower Coordination

A common approach is to use a leader or formation center. The other agents calculate their positions relative to this leader. When the leader moves or rotates, the formation slots move with it. This makes the group easier to control because the squad can be directed through a single reference point.

### Role-Based Positioning

Role-based positioning gives each agent a purpose within the formation. For example, stronger units may be placed at the front, ranged units may stay behind, and support units may remain near the center. This makes the formation more meaningful than a simple visual pattern.

### Believable Adaptation

A believable formation system should not force agents to stay in perfect positions at all times. In dynamic environments, agents may need to avoid obstacles, wait for space, temporarily move out of formation, or rejoin after being blocked. Small imperfections can make the group look more natural, as long as the overall formation remains readable.

## Proposed System

The proposed system consists of a squad manager and multiple AI agents.

### Squad Manager

The squad manager is responsible for the high-level coordination of the group. It stores the squad members, the active formation type, the leader or formation center, and the role assigned to each agent.

Possible responsibilities:

- Keep track of all squad members.
- Store the current formation.
- Assign each agent to a role.
- Calculate formation slot positions.
- Update target positions when the squad moves.
- Decide when the formation should change.

### AI Agents

Each AI agent is responsible for moving toward its assigned formation slot. The agent uses Unreal Engine's navigation system to reach the target position. If the slot is blocked or unreachable, the agent can temporarily move to a fallback position and later try to return to its original slot.

Possible responsibilities:

- Receive a target slot from the squad manager.
- Move toward the assigned slot using AI movement.
- Avoid obstacles using Unreal's NavMesh.
- Detect when the assigned slot cannot be reached.
- Rejoin the formation after avoiding an obstacle.

## Implemented Prototype

The Unreal Engine prototype is implemented in `Source/GameAIProg/SquadCoordination/Level_SquadCoordination.cpp`. The current version includes:

- Role-based squad members: leader, left flank, right flank, and rear support.
- Formation layouts: wedge, column, and line.
- A shared squad target set by mouse input.
- Per-agent formation slots calculated from the squad target and active formation.
- NavMesh projection for formation slots so agents receive reachable movement targets when possible.
- A baseline comparison mode where all agents move to the same target instead of using formation slots.
- Local avoidance through an avoidance-aware arrive steering behavior.
- Behavior tree states for formation following, rejoining, low-health fallback, ally support, and patrol behavior.
- A patrol enemy used to test low-health fallback and support behavior.
- Runtime research metrics shown in the ImGui panel.

### Runtime Research Metrics

The debug panel reports values that can be used in the paper:

| Metric | Meaning |
| --- | --- |
| Average slot error | Average distance between each agent and its assigned target slot. |
| Max slot error | Worst current distance between an agent and its assigned slot. |
| Average spacing error | How far the current distances between agents differ from the desired formation distances. |
| Stuck agents | Number of agents that are no longer making enough progress toward their assigned slot. |
| Relaxed slots | Number of agents temporarily using a fallback slot after being detected as stuck. |
| Settle time | Approximate time after a new target before the formation is considered close enough to its slots. |

These metrics are intended for comparison between the role-based formation mode and the shared-target baseline.

## Formation Slot Example

Formation slots can be stored as local offsets from the leader or formation center.

```text
Wedge formation

          Leader
        /        \
 Left Flank    Right Flank
        \        /
        Rear Support
```

Example slot offsets:

| Role | Local Offset |
| --- | --- |
| Leader | `(0, 0)` |
| Left Flank | `(-300, -300)` |
| Right Flank | `(300, -300)` |
| Rear Support | `(0, -600)` |

These local offsets can be converted into world positions using the leader's position and rotation. Each agent then receives its world-space slot as a movement target.

## Unreal Engine Implementation Plan

1. Create a squad manager actor or component.
2. Register all squad AI agents with the squad manager.
3. Assign each agent a role inside the squad.
4. Define formation data as local offsets.
5. Convert formation offsets into world-space positions.
6. Project each target position onto the NavMesh.
7. Send each AI agent to its assigned target position.
8. Detect blocked or unreachable slots.
9. Use fallback positions when agents cannot reach their assigned slot.
10. Allow agents to return to their original slot after the obstacle is cleared.

## Adaptability Strategies

### Formation Compression

When the squad moves through narrow spaces, the distance between agents can be reduced. This helps the group pass through corridors without completely breaking the formation.

### Formation Switching

The squad can switch formations depending on the environment. For example, a wedge formation can be useful in open areas, while a column formation can work better in corridors.

### Temporary Slot Relaxation

If an agent cannot reach its exact slot, it can move to a nearby valid position instead. This prevents the agent from getting stuck while still keeping it close to the group.

### Rejoining Behavior

After avoiding an obstacle, the agent should try to return to its original role slot. This helps the formation recover naturally after being disrupted.

### Implemented Stuck Recovery

The prototype detects a stuck agent by checking whether it is still far from its target slot while making very little movement progress for a short amount of time. When this happens, the agent temporarily uses a relaxed fallback slot closer to the squad center. Once it starts moving again or gets close enough, it can return to its normal formation slot.

This does not solve every possible navigation problem, but it gives the system a measurable recovery behavior that can be discussed and tested in the paper.

## Evaluation Plan

The system can be evaluated using different test scenarios:

- Open area movement.
- Movement through narrow corridors.
- Movement around static obstacles.
- Formation switching between open and narrow spaces.
- One agent being blocked while the rest of the squad continues.
- Recovery after the formation is disrupted.

For each scenario, run the test twice:

1. Shared target baseline: every squad member receives the same destination.
2. Role-based formation: each squad member receives an assigned formation slot.

Record the runtime metrics from the ImGui panel for both runs. The most useful values for comparison are average slot error, max slot error, average spacing error, stuck agent count, relaxed slot count, and settle time.

Example result table:

| Scenario | Mode | Avg slot error | Max slot error | Avg spacing error | Stuck agents | Relaxed slots | Notes |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| Open area | Shared target baseline | | | | | | Agents tend to cluster near the same point. |
| Open area | Role-based formation | | | | | | Formation should remain readable. |
| Obstacle path | Shared target baseline | | | | | | Watch for crowding around the obstacle. |
| Obstacle path | Role-based formation | | | | | | Watch how many agents recover using relaxed slots. |

Possible evaluation questions:

- Does the squad keep a recognizable formation?
- Do agents avoid blocking each other?
- Can the squad move around obstacles without getting stuck?
- Do agents return to formation after disruption?
- Does the movement look believable from the player's perspective?
- Is the system easy to expand with new formations or roles?

## Suggested Paper Structure

1. Introduction: explain why squad movement needs coordination instead of independent agents.
2. Research question: use the question at the top of this README.
3. Background: steering behaviors, leader-follower movement, formations, behavior trees, blackboards, and NavMesh.
4. Method: describe the role-based formation system, baseline mode, behavior tree states, and stuck recovery.
5. Implementation: explain how Unreal calculates slots, projects them to the NavMesh, and sends them to AI controllers.
6. Evaluation: compare shared-target baseline against role-based formation using the test scenarios and metrics.
7. Results: include screenshots, tables, and observations.
8. Discussion: explain what improved, what failed, and what still feels unnatural.
9. Conclusion: answer whether the system creates believable coordination and adaptability.

## Current Limitations

- The squad manager logic currently lives in the level script instead of a reusable squad manager actor or component.
- Formation switching is manually controlled from the debug panel; it is not yet automatically selected from corridor width or open-space detection.
- The stuck recovery is rule-based and simple. It detects low movement progress but does not reason about the full path.
- The enemy interaction is a test stimulus for support and fallback behavior, not a full combat system.
- Roles currently influence formation position and support/fallback behavior, but they do not yet contain deeper tactical actions such as suppressing, flanking around cover, or coordinated attacks.

## Expected Conclusion

Formation movement for a squad of AI agents can be implemented in Unreal Engine by combining centralized squad coordination with individual agent navigation. A squad manager can define formation structure through roles and relative slot positions, while each AI agent uses Unreal's navigation system to move toward its assigned target. Believable coordination comes from allowing agents to adapt locally, temporarily break formation when necessary, and rejoin the group once the path is clear.

The comparison against a shared-target baseline is expected to show that role-based slot assignment produces more readable group movement and less visual clustering. The trade-off is that coordinated movement requires extra logic for blocked slots, recovery, and formation tuning.

## Sources To Research

These are the main topics that should be researched further while writing the final paper:

- Steering behaviors for autonomous characters.
- Leader-follower movement.
- Formation control in games.
- Role-based group AI.
- Unreal Engine AI Controllers.
- Unreal Engine Behavior Trees and Blackboards.
- Unreal Engine Navigation Mesh.
- Environmental Query System for finding valid positions.

Recommended references to include in the final paper:

- Craig Reynolds, steering behaviors for autonomous characters.
- Ian Millington and John Funge, *Artificial Intelligence for Games*.
- Mat Buckland, *Programming Game AI by Example*.
- Unreal Engine documentation for AIController, Behavior Trees, Blackboards, and Navigation Mesh.
