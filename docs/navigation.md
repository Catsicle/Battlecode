
 these are the things i want to implement as of now, i want to implement a path planning algorithm that will order a bot to travel from point a to point b efficeintly , its logic is written in detail below , also for a regular bot ,there will be a normal navigation strategy which i also have attached in the file, its goal would just be to move in the area ensuring it doesnt visit those cells which dont have any pearls,its exact logic of creating that are given in the prompt file , the first way of navigation will be triggered only when a bot receives a particluar sonar of that mesage type with that start and end location , as of now just simulate and put its code so that it can be reused in future , for the normal navigation bot ,initially all bots will follow this strategy apart from queen , explorer bots , so basically keep this as the fallback logic as of now,all bots will create a map of the area they have seen till now ,the detailed instructions of creating that map in memeory is given in persistent memory section in the prompt file, create these files as of now , keep rest as placeholder only , tell me exactly what things i need to add file by file , also write a doc file which details the things u have done so that an agent can understand it ,,for the directory structure u have the freedom to choose it considering in mind the discussions we have had for it in the previous chat , detail the dir structure also to me which u will follow

```markdown
a bit about the sonar message structure(a sonar message can be only 64 bit long)
in each message 10 bits will be used to record the message id(4 bits) and the rest 6 bits will be used for authentication , i have not decided on the authentication part yet, so just keep a placeholder for it 
the rest space will be used for diffent types of data for different types of messages 

```

## 0. Mandatory reading BEFORE coding
Read local helper.hpp and these pages (https://game.battlecode.au/docs/<page>): overview, map-info, pearls, kelp-and-portals, vision, movement, splitting, sonar, death, execution-order, timeouts, helper, libraries.
- If this prompt conflicts with the docs, the docs win. Record every conflict in NOTES.md.
- create a separate file which details each and evry thing u have done which is understandable by a human 
-create a separate file which details each and every thing u have done which is understandabel to  an agent 

#a target coordinate will be delivered to the bot with the help of sonar , as of now just assume that it will come and assuming just implement the naviation file from start point to end point
## 1. Facts from the docs
- Grid W,H in [10,64], toroidal (edges wrap). Tile index = y*W+x.
- Kelp and portals live on EDGES between tiles. Crossing kelp = death. Portals come in pairs sharing an id; entering one emerges from its partner. Vision does NOT pass through portals, so the exit side is usually unseen. A portal can exit straight into kelp.the two connected portals are placed in similar orientation(both either in vertical or in horizontal),entering along right side makes the dragon exit from the left side from the other connected portal,
- Vision: 7x7 square centred on head (Chebyshev <= 3), wraps. Each tile gives pearl presence, pearl countdown (-1 = never spawns), dragon part (team, id, is_head), 4 edges.
- Each dragon outputs actions; only the last counts. Moves: make_move(dir) or make_moves(list). Free steps per turn: s = ceil(L/4), L fixed at turn start. Each step beyond s costs 1 segment; if it cannot be paid the dragon dies.
- Collision is checked at EVERY step before that step's movement. Death if: kelp edge crossed; target tile holds ANY dragon segment (own, ally, enemy; own tail counts even if it would vacate this step); target holds a head (both die).
- Eating a pearl: tail does not advance that step (L+1).
- Pearl countdown decrements at round start; a spawn succeeds only on an empty tile; the next gap is random in a hidden [min_gap,max_gap]. Mirrored tiles share countdowns.
- Maps are symmetric: vertical mirror, horizontal mirror, or 180 deg rotation.
- Budget: 100M CPU points per dragon per turn (aim for <= 15M worst case). Loads/stores 2, arithmetic 1, call 4, div 3. Every stdout/stderr write costs 2.5M. NO logging in release builds; guard debug output with `#ifdef NAV_DEBUG`. Use flat arrays, no std::map/set/iostream in hot paths. Each dragon is a separate process, so globals persist across turns but are not shared; the only cross-dragon channel is sonar.

## 2. Notation
W,H,N=W*H. Head h. Body B[0..L-1], B[0]=head. e = pearls eaten so far along a candidate path. s = ceil(L/4).
Path steps t = 1,2,...; turn index of step t: rho(t) = floor((t-1)/s) (rounds elapsed before step t executes).
Dist(u,v) torus geodesic on the known graph (BFS), not Manhattan.
All costs are integers, fixed-point x4 (1.0 step = 4 units).

## 3. Persistent map memory (per dragon, flat arrays, updated every turn)
- kelp[N] : uint8 bitmask of N,E,S,W kelp edges; known_edge[N]: uint8 mask of edges actually observed.
- portal_id[N][4] : int16, -1 if none. portal_exit_known[...] : whether the partner side has been observed.
- seen[N] : bool. last_seen_round[N].
- pearl_spawn_round[N] : absolute round = now + countdown when last observed (INF if -1). never_spawns[N].
- visits_nopearl[N] (n_v), last_visit_round[N], eaten_here[N] (E_v: a pearl was eaten on this tile at least once).
- occ[N] : last observed dragon part (team, id, segment index from head, owner length, round seen). Treat entries older than 2 rounds as stale unless in current vision.
- Optional Phase 2 (behind `ENABLE_SYMMETRY`): keep 3 symmetry hypotheses, eliminate any contradicted by an observed tile/edge pair, and once exactly one remains, mirror kelp/portal/pearl-timer knowledge into the partner tiles (edges swap accordingly). Use the transform from docs/map-files and map-info (VERIFY).

## 4. Cost model
Hard(v,t) = TRUE (forbidden, never merely "expensive") if ANY holds:
 (a) the edge crossed into v has kelp;
 (b) own body: v == B[j] and j <= L + e - t;
 (c) ally/enemy body: v holds segment j of dragon D (length L_D, seen a rounds ago) and rho(t) + a < L_D - j (pessimistic: assume it vacates 1 tile/round);
 (d) rho(t) == 0 and some other dragon's head h' (any team) has Dist(h',v) <= s_D' (it could end its free steps on v);
 (e) portal edge whose exit tile/exit-side edge has not been observed (Navigate mode only; in Explore mode this is Soft, see below).
Soft cost added to a step into v at step t (units x4):
 step(v) = C_STEP (=4)
         + C_PORTAL_EXTRA (=2) if the move uses a portal
         + C_UNKNOWN (=6) if v is unseen (Navigate mode only)
         + pen(v)  (Explore mode only)
         + C_PORTAL_UNKNOWN (=40) if portal with unobserved exit (Explore mode only)
         + danger(v,t)
 danger(v,t) = D0 (=120) / (1+rho(t))^2   if rho(t) >= 1 and some enemy head h' has Dist(h',v) <= (rho(t)+1)*s_D'; else 0.
 Own new body: with strictly positive step costs, shortest paths are simple, so path self-collision cannot occur; assert this in debug builds.
Revisit penalty (Explore mode):
 pen(v) = 0 if eaten_here[v]
 pen(v) = min(P_MAX (=48), LAMBDA (=8) * visits_nopearl[v] * K / (K + age_v)) otherwise, where age_v = now - last_visit_round[v], K = 150.
 visits_nopearl[v] increments only when the head ENTERS v and no pearl is eaten there. Set eaten_here[v]=true when a pearl is eaten on v.
 Config flag EXEMPT_SPAWNABLE (default false): if true, also exempt tiles with pearl_time >= 0 (they can respawn pearls). Make it easy to toggle for A/B testing.

## 5. Search engine (one routine for both modes)
Dijkstra with a Dial bucket queue (integer costs, ring of C_MAX+1 buckets) over tiles, on flat arrays with a reusable generation-stamped dist[] to avoid clearing. Neighbour expansion: 4 directions, check kelp edge mask, portal mapping, then Hard(v,t) and the soft cost. Labels carry: cost, steps t, e (pearls eaten on path, from known pearls). Node key = tile, EXCEPT tiles in the own-body set, where key = (tile, min(t, HORIZON_T=48)) so a tile blocked at its earliest arrival can still be entered later. Keep parent pointers and reconstruct paths. No heap allocation in the turn loop.

## 6. Safety layers (a candidate move prefix M of x steps is accepted only if all pass)
 L1 Exact simulation: replay M step by step on a copy of the body using the collision rules in section 1 (including portals and pearl eating). Any death = reject.
 L2 Time-indexed own-body blocking: already enforced in Hard(b); L1 re-verifies it independently.
 L3 Lookahead survival: Surv(state, K) = exists a move that is L1-safe and Surv(next, K-1), K = 3 (DFS, branching <= 3). Must be true from the post-M state, using only known tiles.
 L4 Trap test: flood fill from the post-M head over known tiles not blocked at post-M occupancy. Accept if (reached tiles >= L + e + 2) OR (the fill reaches any frontier tile = known tile with an unseen neighbour).
If any layer fails, try in order: (i) shorten M by one step and recheck; (ii) next-best destination/path (re-run the search with the failed first edge forbidden); (iii) safest single move: among L1-safe moves pick max (L3 depth reached, then L4 area); (iv) emergency fallback in section 9.

## 7. Explore mode (no target)
Run section-5 Dijkstra from the head over KNOWN tiles. For every reachable known tile v with cost D(v) and steps t(v):
 G(v) = B_PEARL (=120) * [pearl present on v]
      + B_SOON (=80) * 0.7 * [no pearl now, pearl_spawn_round[v] <= now + rho(t(v)) + 1, not never_spawns]
      + B_FRONT (=30) * [v is frontier]
      + B_STALE (=20) * min(age_v, 200) / 200
 S(v) = G(v) - D(v).  Choose v* = argmax S. If max S <= 0, choose the reachable frontier tile with the least D; if none, the tile with the largest age_v.
Dijkstra only supplies path costs; the destination is chosen by S. Tie-break equal S by more pearls on the path, then fewer steps.
Then take the path to v*, set x = min(s, path length), and run section-6 checks on the first x steps.

## 8. Navigate mode (start and target known)
Objective: minimise total step cost from head to target T under Hard constraints and the section-6 safety layers. There are NO pearl, penalty or exploration terms. Unseen tiles are optimistic with cost C_UNKNOWN, but commit only moves through tiles that are currently known and L1-safe; re-plan every turn as vision updates.
Free sprint: emit x = min(s, safe-committable prefix length) moves in ONE make_moves call per turn. Never use paid steps except in the section-9 emergency. Prefer routes that need fewer turns, turns = ceil(n/s) where n is the step count; tie-break turns, then total cost. The spare free steps in the final turn should be used on the safest tiles, never on riskier shortcuts.
Death prediction requirement: before emitting ANY move, the section-6 layers must pass for the committed prefix. If the globally cheapest path fails them, the agent must switch to the next-cheapest path that passes. It must never emit a prefix that fails L1, regardless of how short the remaining route is.

## 9. Emergency fallback (every neighbour is Hard / no prefix passes)
1. Prefer a move that is only soft-risky (uncertain enemy reach) over a certain-death move.
2. If none, and L >= 2 and docs/splitting permits: SPLIT as a last resort. Intent: the front part (length 2) goes forward, the remaining body splits with its new head facing backwards along the trail, which is free space. when splitting u will always split into lenght 2 and original length-2 legnth of dragons , the small dragon will be the parent and the large dragon which will start its head from where the tail  of dragon was there initially , the child gets a new id , and the small parent which is still moving in the same direction as previously remains with the old id . Splitting shrinks the queen, so never split while any L1-safe move exists.
3. Otherwise output any legal move.

## 10. Per-turn pipeline
read round -> update map memory from the 7x7 window and sonar -> detect eaten pearl / visit counters from last turn's result -> choose mode (Navigate if a target is available, else Explore) -> section 7 or 8 search -> section 6 safety -> emit moves -> (optionally)(dont send any sonar as of now, it will be discussed about but later) send sonar with newly seen kelp/portal data (VERIFY sonar format and cost).



