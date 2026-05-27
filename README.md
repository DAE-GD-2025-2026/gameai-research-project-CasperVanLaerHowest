# Squad Coordination AI in Unreal Engine

## Research Question

How can formation movement be implemented for a squad of AI agents in Unreal Engine while maintaining believable coordination and adaptability in dynamic environments?

## Portfolio Piece Description

This project researches and prototypes a squad coordination AI system in Unreal Engine. The system focuses on dynamic formation movement, role-based positioning, obstacle adaptation, and believable group behavior for AI-controlled agents.

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

## Evaluation Plan

The system can be evaluated using different test scenarios:

- Open area movement.
- Movement through narrow corridors.
- Movement around static obstacles.
- Formation switching between open and narrow spaces.
- One agent being blocked while the rest of the squad continues.
- Recovery after the formation is disrupted.

Possible evaluation questions:

- Does the squad keep a recognizable formation?
- Do agents avoid blocking each other?
- Can the squad move around obstacles without getting stuck?
- Do agents return to formation after disruption?
- Does the movement look believable from the player's perspective?
- Is the system easy to expand with new formations or roles?

## Expected Conclusion

Formation movement for a squad of AI agents can be implemented in Unreal Engine by combining centralized squad coordination with individual agent navigation. A squad manager can define formation structure through roles and relative slot positions, while each AI agent uses Unreal's navigation system to move toward its assigned target. Believable coordination comes from allowing agents to adapt locally, temporarily break formation when necessary, and rejoin the group once the path is clear.

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

