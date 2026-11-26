# `tests/` — test suite

**Framework:** [Catch2](https://github.com/catchorg/Catch2) v3.8.1, downloaded automatically by CMake.
**Executable:** `pokebot_tests` (links `pokebot_lib` and `pokebot_import_lib`).

No test touches the network. Code that would use the network is tested through a fake HTTP client.

## Running

```bash
ctest --preset debug
```

Run one group, with Catch2's full output:

```bash
./build/debug/tests/pokebot_tests.exe "[convert]"
```

List every test:

```bash
./build/debug/tests/pokebot_tests.exe --list-tests
```

## Files

| File | Tag | Tests | Covers |
|---|---|---|---|
| [`test_decklist.cpp`](../../tests/test_decklist.cpp) | `[decklist]` | 5 | Parsing names with spaces/apostrophes/`{D}`, Windows line endings, total mismatch, error line numbers, both project decklists load with 60 cards |
| [`test_tcgdex.cpp`](../../tests/test_tcgdex.cpp) | `[tcgdex]` | 8 | Padded card ids, unknown sets and numbers, cache then offline reuse, retries after empty/failed responses, giving up, offline with no cache; whole sets in pages, cached, and failing cleanly; set codes found through the series |
| [`test_convert.cpp`](../../tests/test_convert.cpp) | `[convert]` | 11 | Pokémon ex with `×` damage and a free attack, damage modifiers surviving save and reload, Mega ex worth 3 Prizes with no Tera check, ACE SPEC detection, Basic Energy type from its name, Special Energy mislabeled as Normal, the newest sets' spellings, notes for missing data, corrections and the repo's corrections file, rejecting unknown data |
| [`test_translate.cpp`](../../tests/test_translate.cpp) | `[translate]` | 12 | The translator: cleaning and splitting text, damage formulas, flips and conditions, damage to chosen Pokémon, deck, discard pile and Energy sentences, lasting effects and switching, texts it mustn't read, Items and Supporters with their requirements, Abilities, Tools, Stadiums and Special Energy, and every hand-written text it reads loading the same, with only the known texts unread |
| [`test_standard.cpp`](../../tests/test_standard.cpp) | `[standard]` | 7 | Coverage of Standard: printing ids, reprints grouped, printings not legal or not numbered left out, effects from another printing or a translation, missing effects and Tera not checked, problems found and fixed by corrections, Energy symbols in names, reprints linked and unlinked, translations stored and taken away |
| [`test_deck_validator.cpp`](../../tests/test_deck_validator.cpp) | `[deck_validator]` | 9 | One test per deck-building rule, all printings of a name counting together, Basic Energy exemption, unknown cards, several problems at once, and every real deck in `data/decks/` (skipped without generated data) |
| [`test_card_db.cpp`](../../tests/test_card_db.cpp) | `[card_db]` | 8 | JSON round trip, effects files adding rules and attack effects, typo'd attack names rejected, `missing_effects` list, `no_effect` Abilities not missing, bad card data rejected, `same_as` using another card's effects, a stored translation used only without an effects file |
| [`test_engine_state.cpp`](../../tests/test_engine_state.cpp) | `[engine]` | 10 | `FixedVector`, `Rng` determinism and shuffles, independent `GameState` copies, card ownership, `CardTable` numbering and illegal decks |
| [`test_engine_setup.cpp`](../../tests/test_engine_setup.cpp) | `[setup]` | 10 | Opening hands, determinism, turn order, Prize cards and turn 1, merged choices and the Bench limit, mulligan bonus draws, 1,000 random real-deck setups, illegal actions, `apply_legal` playing the same game as `apply` |
| [`test_engine_turns.cpp`](../../tests/test_engine_turns.cpp) | `[turns]` | 5 | Ending the turn, drawing, alternating turns, flag resets, deck-out |
| [`test_engine_actions.cpp`](../../tests/test_engine_actions.cpp) | `[actions]` | 10 | Benching, one Energy per turn, evolution timing, retreating (with and without a choice, free, not allowed), 200 random real-deck games (the Bench never over `rules::bench_size`) with a summary of how they ended |
| [`test_engine_attacks.cpp`](../../tests/test_engine_attacks.cpp) | `[attacks]` | 11 | Cost matching, no attack on turn 1, damage, Weakness and Resistance, copied attacks using the attacker's type, Pokémon ids, Knock Outs and promotion, ex / Mega ex Prize cards, every way to win including draws |
| [`test_rules.cpp`](../../tests/test_rules.cpp) | `[rules]` | 4 | Rulebook answers for HP and Knock Outs, type, costs, Energy, Prize cards, turn limits and evolution timing |
| [`test_engine_events.cpp`](../../tests/test_engine_events.cpp) | `[events]` | 5 | Events of an attack that Knocks Out (in order), of benching, evolving, attaching and retreating; history of the last two turns; the extra-attack decision |
| [`test_effect_program.cpp`](../../tests/test_effect_program.cpp) | `[effects]` | 16 | Compiling effects: damage first, `search_deck`, `may` and `if` expansion, placed damage, Trainer programs, conditions, `special_condition`, rejected mistakes, every filter field, errors naming the attack |
| [`test_engine_effects.cpp`](../../tests/test_engine_effects.cpp) | `[effects]` | 10 | Running effects: draw, search, filters, choosing nothing, yes/no, Energy to hand, switching, discard and draw, Knock Outs waiting for the effect, copying a game mid-effect |
| [`test_engine_trainers.cpp`](../../tests/test_engine_trainers.cpp) | `[trainers]` | 12 | Items resolving and going to the discard pile, one Supporter per turn and none on turn 1, Stadiums replacing each other, one Tool per Pokémon, `"requires"` and unwritten cards, Ultra Ball, Boss's Orders, Poffin to the Bench, `if`, Judge, Night Stretcher's filter |
| [`test_engine_conditions.cpp`](../../tests/test_engine_conditions.cpp) | `[conditions]` | 11 | Asleep and Paralyzed blocking attacks and retreats, an attack applying Asleep, which conditions replace or stack, Active only, retreating and evolving clearing them, Poison and Burn counters, a Poison Knock Out, recovering on heads, Paralysis timing, confusion failing on tails |
| [`test_engine_damage.cpp`](../../tests/test_engine_damage.cpp) | `[damage]` | 11 | Damage formulas: × per card in hand, + per Benched Pokémon, flip until tails, an "if you do" bonus, discarding Energy from several Pokémon, × per damage counter; Red Card; Cipher's deck order; the compile rules for formulas |
| [`test_engine_targets.cpp`](../../tests/test_engine_targets.cpp) | `[targets]` | 10 | Weakness on the Active but not the Bench, Tera Bench protection, two different targets, the damage-counter spread order, damage by the target's counters, Flamebody Cannon, Tuck Tail and promoting, copying an N's attack (and with nothing to copy), Crushing Hammer into the owner's discard pile |
| [`test_engine_damage_effects.cpp`](../../tests/test_engine_damage_effects.cpp) | `[damage_effects]` | 8 | Healing this Pokémon, a chosen damaged one or each of yours; an Item that heals needing a damaged Pokémon; damage to itself; every Benched Pokémon at once; Weakness and Resistance ignored; conditions on the opponent's Active; counting Pokémon in play and the counters on the opponent's Active; discarding an Energy from the opponent's Active |
| [`test_engine_protection.cpp`](../../tests/test_engine_protection.cpp) | `[protection]` | 7 | Less damage next turn and then not; all damage prevented, or up to an amount; effects prevented (Special Conditions, damage counters); the Defending Pokémon doing less or taking more; this Pokémon's attacks doing more next turn; the Defending Pokémon's attack only on heads; passives: a damage-reducing Tool, prevention from Pokémon ex, on the Bench, and less damage while Active |
| [`test_engine_lasting.cpp`](../../tests/test_engine_lasting.cpp) | `[lasting]` | 9 | "Can't use that attack" and "can't attack" next turn, retreating ending an effect, "can't retreat" for one turn, the Item lock, a damage bonus only against an Active ex, this-turn effects ending, attack Knock Outs versus Checkup ones in history, the compile checks |
| [`test_engine_passive.cpp`](../../tests/test_engine_passive.cpp) | `[passive]` | 7 | A Tool changing only its holder, a Stadium reaching both players' N's Pokémon, Skyliner on your Basic Pokémon only, Fairy Zone adding Weakness and Shred ignoring it, Binding Mochi only while Poisoned, passive lists counting as written, the compile checks |
| [`test_engine_abilities.cpp`](../../tests/test_engine_abilities.cpp) | `[abilities]` | 10 | Once per turn and again next turn, Active only, one per name, requirements and costs, damage counters Knocking Out at once, the top 2 cards, shuffling the Active away, moving counters, a filtered switch that Poisons, a Stadium action once per turn |
| [`test_engine_triggers.cpp`](../../tests/test_engine_triggers.cpp) | `[triggers]` | 8 | A trigger asking yes/no then searching, not offered as an Ability to use, one per name, unmet requirements, discarding the Stadium to its owner, switching in and moving Energy, Risky Ruins on Basic non-{D} Pokémon (Poffin too), Poffin not counting as "played", trigger names |
| [`test_engine_energy.cpp`](../../tests/test_engine_energy.cpp) | `[energy]` | 7 | A Special Energy's usual Energy and its holder-dependent Energy, one card providing several, a cost a greedy match would miss, Retreat paid in Energy provided, the discard choice stopping once paid, the end-of-turn discard, the load checks |
| [`test_engine_attach.cpp`](../../tests/test_engine_attach.cpp) | `[attach]` | 6 | Energy from the discard pile to the only matching Benched Pokémon, each of 2 targets getting its own Energy from the deck and Poison for the Active, no Poison without it, up to 2 Energy only with more Prize cards left, moving Energy between your Pokémon, the compiled loop |
| [`test_engine_custom.cpp`](../../tests/test_engine_custom.cpp) | `[custom]` | 7 | Rare Candy straight to Stage 2, not on the first turns or a Pokémon played this turn; Transformation Tome needing two, keeping Energy and damage, and Knocking Out a Basic with less HP; Area Zero's 8 spaces with a Tera Pokémon and the discard down to 5 when it leaves; the load checks |
| [`test_engine_knowledge.cpp`](../../tests/test_engine_knowledge.cpp) | `[knowledge]` | 9 | The open zones and hands, face-down setup, a revealed search known until shuffled away, a search that doesn't reveal, searching the deck showing all of it to its owner, a card picked up from the discard pile, looking at the top of the deck, a mulligan showing the hand, and 100 random games checking every visible card is recorded and no Prize card is known |
| [`test_observation.cpp`](../../tests/test_observation.cpp) | `[observation]` | 14 | Your own hand and only the size of the opponent's, definitions only for seen cards, the deck and Prize cards as one sorted list, a revealed card in the opponent's view, cards on the bottom of the deck shown where they are, the decision stack and an effect's chosen cards hidden from the opponent, face-down setup, games differing only in hidden cards looking the same (and a visible change showing), random games on the sample cards and the real decks naming only cards the viewer knows, `describe(observation)`, `play_game` showing each agent its own view, and the actions offered naming only cards the deciding player knows |
| [`test_invariants.cpp`](../../tests/test_invariants.cpp) | `[invariants]` | 5 | The checker catches duplicated, missing and misplaced cards, missed Knock Outs, bad evolutions and Energy, and a missing Active |
| [`test_agents.cpp`](../../tests/test_agents.cpp) | `[agents]` | 3 | Random choices stay legal and cover every option, the biased agent attacks, seeds repeat (given an empty observation) |
| [`test_world.cpp`](../../tests/test_world.cpp) | `[world]` | 9 | A world built from an observation observes back to it, passes the invariant checker and offers the same actions (sample cards and real decks, every action after setup, both players); your deck and Prize cards dealt from the same cards in different ways; the opponent's unseen cards as `unknown_card()`; attaching, benching and retreating looking the same as in the game; a card the opponent is discarding from hand stood in for from their hand; no world during setup; with a deck model on the real decks, the same guarantees with the opponent's unseen cards dealt only from the list drawn, several lists drawn, and never the opponent's list when the library leaves it out; without a model, exactly the world as before |
| [`test_weights.cpp`](../../tests/test_weights.cpp) | `[weights]` | 9 | Weights files: saved and loaded exactly, the hand weights' file equal to the default and M10's to the learned weights; which agents use the learned weights by default; line endings and order, wrong files rejected; `set_weights` setting every copy in an agent's options, and `set_playout_weights` the playouts' policy; on the real decks, the loaded hand weights playing the default's games |
| [`test_evaluation.cpp`](../../tests/test_evaluation.cpp) | `[evaluation]` | 10 | Finished games scored 1, −1 or 0; an even position about 0; a Prize card, damage and Energy scoring higher and Poison lower, with the expected feature values; a player choosing a new Active scored by their healthiest Benched Pokémon; the hand counting up to 7 cards and the deck up to 30; the last 8 cards of the deck, and evolutions by stage; only the wanted features worked out, for exactly the same evaluation; random games with exactly opposite features in [−1, 1]; on the real decks, a world giving the same evaluation as the game |
| [`test_greedy.cpp`](../../tests/test_greedy.cpp) | `[greedy]` | 7 | Taking a Knock Out; attaching Energy rather than ending the turn, and scoring it higher; an Ability that discards then draws, with an empty deck, scored by its discard and not used; going first and putting down the Basics with the most HP during setup; the same seeds playing the same game; whole sample-card games keeping every invariant; beating the random agent in at least 3 of 4 real-deck games from either seat |
| [`test_arena.cpp`](../../tests/test_arena.cpp) | `[arena]` | 7 | The Wilson interval's known values; a tally from either seat with draws as half; every pairing played equally with each seed in both seats and the totals adding up; kept games playing back to the same result; the same results with 1 and 4 threads; an agent's failure stopping the arena; progress reported and each side's counters added up |
| [`test_action_class.cpp`](../../tests/test_action_class.cpp) | `[planner]` | 4 | Attaching, benching, evolving and retreating deterministic; ending the turn, attacking and a Knock Out (without chance) final; a draw, a search, a coin-flip attack and ending the turn while Asleep uncertain; on the real decks, the same class in two worlds, the same view after a deterministic action and the same value after a final one |
| [`test_state_hash.cpp`](../../tests/test_state_hash.cpp) | `[planner]` | 4 | Equivalent orders of the same actions reaching the same state and hash; the hash changing with what decides the game but not with the random number generator or the hand's order; on the real decks, equal hashes meaning equal states; a closed set holding each state once |
| [`test_plan.cpp`](../../tests/test_plan.cpp) | `[planner]` | 5 | `apply_plan` in order; `apply_plan_checked` stopping at an action that isn't legal; a final plan's value e after it with any number of samples; a draw worth e after it in every sampled world, and a search scored after taking a Pokémon; a coin-flip attack averaged over heads and tails, the same for the same state and seed |
| [`test_or_tree.cpp`](../../tests/test_or_tree.cpp) | `[planner]` | 6 | Attach and attack for a Knock Out; evolve, attach, attack; a free retreat then the other Pokémon's attack; a win by the last Prize card and k Prize cards; no plan when none exists, the closed set saving nodes, and the budget; on the real decks, every plan found legal and taking a Prize card |
| [`test_and_tree.cpp`](../../tests/test_and_tree.cpp) | `[planner]` | 8 | The best plan evolving, attaching and attacking; the best worth at least any one plan-ending action; top-K best first and every end state; the closed set and f_bound expanding fewer nodes for the same best value; f_bound with evolved and deck-out weighted; the upper bound at least e, 1 for a winning Knock Out, and its evolved and deck-out terms; on the real decks with both sets of weights, the bound at least e, the best plan legal and worth its value, and f_bound not changing it |
| [`test_planner.cpp`](../../tests/test_planner.cpp) | `[planner]` | 6 | A win two actions away; the and-tree's best plan played to the end from one plan; planning again when the observation isn't what the plan expected; whole sample-card games, the same for the same seed; whole real-deck games against random; its search counts and times |
| [`test_planner_lab.cpp`](../../tests/test_planner_lab.cpp) | `[planner]` | 2 | On sample cards and the real decks, the planner lab's counts adding up: no false wins, the closed set and f_bound never expanding more nodes, f_bound never changing the best value, and on the real decks every win found within the Planner's budget |
| [`test_deck_model.cpp`](../../tests/test_deck_model.cpp) | `[deck_model]` | 17 | The archetype library: cards counted by id, lists in order by archetype and variant, illegal lists, unknown cards, cards without effects and repeated lists rejected with the reason, loading from a folder per archetype and each way that fails; the project's library (skipped without generated data): every deck in `data/decks/` as an archetype's `main` list, at least two variants each, no two lists the same. The belief: the prior with nothing seen, a card one archetype plays settling it, a tech card picking the variant, copies counted hypergeometrically, unexplained cards keeping it defined, sums by archetype, bad arguments; on the real decks, the same cards seen giving the same belief whatever the real list, and the opponent's archetype found, even with its exact list left out of the library. Drawing lists as often as the belief says, and dealing a list's cards less the ones seen, with unexplained cards leaving out random ones |
| [`test_playout.cpp`](../../tests/test_playout.cpp) | `[playout]` | 9 | Depth 0 scoring the state as it is; stopping at the start of the player's r-th next main phase, from their turn and from the opponent's; a playout to the end scored with u; the same seed playing the same playout and others different, under both policies; playouts played with other weights than they are scored with; `max_actions` and a negative depth; each player deciding from its own observation, the opponent never shown the searcher's hand; policies by name; on the real decks, games differing only in cards the searcher can't see giving exactly the same playouts |
| [`test_flat_mc.cpp`](../../tests/test_flat_mc.cpp) | `[flat_mc]` | 9 | A win taken without playouts; several plans each played out equally, ranked by mean and the best followed; one plan played without playouts; whole sample-card games, the same for the same seed; whole real-deck games against random; games differing only in hidden cards getting the same choice and means; counters and timings; the defaults tuned in 7.6; with `search_choices`, decisions outside the main phase played out and games still repeatable |
| [`test_info_set.cpp`](../../tests/test_info_set.cpp) | `[is_mcts]` | 5 | The information-set key on the real decks: equal observations and the hand's order; the same Energy attached under different numbers in two worlds hashing the same; changes the viewer can see changing it; a different or unseen card changing it; numbering one player's cards telling apart copies the plain key treats alike |
| [`test_tree.cpp`](../../tests/test_tree.cpp) | `[is_mcts]` | 6 | The IS-MCTS tree: children by key, backpropagation for both players, UCB1 with unvisited children first and availability counts, visits concentrating on the minimax moves of a small made-up game, a sometimes-legal move counted available only when it is |
| [`test_macro_actions.cpp`](../../tests/test_macro_actions.cpp) | `[is_mcts]` | 4 | IS-MCTS's moves: none in a finished game; on the real decks, legal plans of the right kind for both players, and the searcher's cached by information set while the opponent's are searched each time; the budget lab's cheap budgets never beating the reference |
| [`test_is_mcts.cpp`](../../tests/test_is_mcts.cpp) | `[is_mcts]` | 7 | The IS-MCTS search: a win two actions away taken; visits adding up to the iterations, at most a move and an outcome node an iteration, legal and cached root plans, the same seed searching the same; on the real decks, positions from both seats; games differing only in hidden cards searched exactly the same; root moves distinct plans whatever their outcomes; matched worlds' visits and skips adding up; `final_by_mean` playing the best mean visited often enough |
| [`test_chance.cpp`](../../tests/test_chance.cpp) | `[endgame]` | 8 | The endgame solver's chance outcomes: one outcome without chance; the opponent's draw by card and copies; a two-card draw against all 120 orders of a 5-card deck; coin flips split in half, flipping until tails capped at `max_flips`; a search as one outcome; a shuffle before a draw sampled; on the real decks, outcomes adding up to 1 and keeping the invariants |
| [`test_expectiminimax.cpp`](../../tests/test_expectiminimax.cpp) | `[endgame]` | 5 | The endgame solver's search in one world: a Knock Out of the last Pokémon worth 1; without pruning every root value exact; alpha-beta and Star1 agreeing with the search without pruning at depths 1 and 2 and visiting no more nodes; the same position solving the same; on the real decks, pruned and unpruned depth-2 searches agreeing, with some cuts |
| [`test_endgame_solver.cpp`](../../tests/test_endgame_solver.cpp) | `[endgame]` | 8 | The endgame solver: one world through `solve_worlds` the same as `expectiminimax`, two copies of a world the same values; pruning over three worlds agreeing with the search without it; a node budget stopping the search; deepening to depth 2, and stopping sooner on a small budget; a Knock Out of the last Pokémon taken; the same seed solving the same; on the real decks, endgames solved from the observation |
| [`test_endgame_agent.cpp`](../../tests/test_endgame_agent.cpp) | `[endgame]` | 8 | The endgame agent: the solver's plan played in the endgame, IS-MCTS outside it; a win taken; a fallback on IS-MCTS on a tiny budget or short of the minimum depth; whole sample-card games, the same for the same seed; whole real-deck games with endgame decisions solved; its counters, then IS-MCTS's |
| [`test_endgame_lab.cpp`](../../tests/test_endgame_lab.cpp) | `[endgame]` | 2 | The endgame lab: placements counted by card, not by copy; on the real decks, world counts, two-segment trees and solves with values in [−1, 1], the same for the same seed |
| [`test_is_mcts_agent.cpp`](../../tests/test_is_mcts_agent.cpp) | `[is_mcts]` | 5 | The IS-MCTS agent: a win taken without searching; one search and its most visited plan played to the end; whole sample-card games, the same for the same seed; whole real-deck games against random; counters and timings |
| [`test_self_play.cpp`](../../tests/test_self_play.cpp) | `[self_play]` | 6 | Self-play games: saved and loaded, not overwritten, bad files rejected; each turn's first main phase and one later, with that moment's features; results for the player to move; the later decision picked evenly and the same each time; on the real decks, features in range |
| [`test_weight_fit.cpp`](../../tests/test_weight_fit.cpp) | `[weight_fit]` | 6 | Fitting weights: known weights and terms recovered from exact chances and from random results; a non-negative fit that no small change improves; more L2, smaller weights; pairing terms that stop a deck-marking feature from getting a weight; terms fitted with the weights left alone |
| [`test_evaluation_lab.cpp`](../../tests/test_evaluation_lab.cpp) | `[evaluation_lab]` | 10 | Evaluation lab: log loss at known and extreme values; a known scale recovered and a backwards one set to 0; errors clustered by seed; no seed scored by a model fitted on it, and no left-out deck fitted on; breakdowns that add up; the adjusted loss below the raw one where only the pairing matters; samples and scores the same on any number of threads, repeated seeds rejected |
| [`test_replay.cpp`](../../tests/test_replay.cpp) | `[replay]` | 10 | Saving and loading, playing back to the same `GameState` and ending (including `play_game`'s own final state) (sample and real decks), illegal actions and wrong results caught, bad files rejected, actions described to each player without the cards they don't know, `print_replay` |
| [`test_client.cpp`](../../tests/test_client.cpp) | `[client]` | 10 | The human agent: the typed number taken, anything else asked again, `b` and `q`, a single choice not asked, no repeated board partway through an effect, a whole game through `play_game`; actions described from an observation reading the same as from the game (sample and real decks); the opponent's Pokémon named as theirs; `print_replay` stopping before each turn |
| [`test_coverage.cpp`](../../tests/test_coverage.cpp) | `[coverage]` | 3 | Every card in our decks has its effects written (the decks directly in `data/decks/`, not every card imported), every search whose text says "reveal" sets `reveal`, and 2,000 biased random games use every attack (copied ones too), every Ability you choose to use, every Trainer, every Stadium action and every Special Energy in our decks, listing whatever was never used |
| [`test_stress.cpp`](../../tests/test_stress.cpp) | `[stress]` | 3 | 1,000 biased and 1,000 uniform random real-deck games with invariant checks, and every state and action can be described. More than 300 of the biased games must end by Knock Out (random players burn through their decks with draw and search Trainers, so deck-outs are common) |

**502 tests in total.** Tests that need the real card data skip themselves if `data/cards/generated/`
doesn't exist yet; run `pokebot-import` first.

## Every card is tested

Milestone 3's sign-off asks for a test per card. It's covered three ways:

- **Chosen effects** (attacks, Abilities you use, Trainers, Stadium actions, Special Energy): the
  coverage test plays them in random games with the real decks, and the stress tests check every
  invariant while they do. Most also have a test of their own with a made-up card. A few can't happen
  with our decks, and the test lists them with the reason: Chien-Pao's Icicle Loop costs {W}{W}{C},
  but `basic-box` has 1 Water Energy (Chien-Pao is there for Snow Sink).
- **Triggered and always-on effects** aren't chosen, so each has its own test: the triggers in
  `test_engine_triggers.cpp`, Tools, Stadiums and passive Abilities in `test_engine_passive.cpp`,
  Area Zero in `test_engine_custom.cpp`, and Neo Upper and Ignition in `test_engine_energy.cpp`.
- **Mechanics no deck card uses** are tested with made-up cards, so they work when a card needs them:

| Mechanic | Test |
|---|---|
| Burned | "Poisoned and Burned put damage counters on at Pokemon Checkup", "Asleep and Burned are each removed on heads" |
| Paralyzed | "Asleep and Paralyzed Pokemon can't attack or retreat", "Paralyzed wears off at the Checkup after its owner's next turn" |
| More than one attack a turn (Festival Lead) | "an extra attack offers that Pokemon's attacks or ending the turn" |
| A Bench over the limit | "when Area Zero leaves play, a player over the limit discards down to 5" |

## Helpers worth knowing

- **`FakeHttp`** (`test_tcgdex.cpp`): queue responses per URL with `queue(path, body)`, and POST responses in order with
  `queue_post(body)`; it keeps the bodies sent in `bodies`. An empty
  body simulates TCGdex's "200 with empty body" glitch; `"!error"` simulates a failed request.
  `calls` counts requests, which is how the cache tests prove nothing was downloaded.
- **`TempDir`** (`test_tcgdex.cpp`): a fresh temporary folder with a `sets.json`, deleted when the
  test ends.
- **`support/test_cards.hpp`**: build made-up cards and decklists: `basic_pokemon`, `evolution`,
  `basic_energy`, `attack`, and `deck({{count, number}, ...})` for set `TST`.
- **`support/real_decks.hpp`**: `load_real_decks()`, the project's real card data and decklists, or
  nothing if `pokebot-import` hasn't been run (the test then skips itself).
- **`support/test_game.hpp`**: arrange exact positions without breaking card accounting:
  `play_setup`, `take_from_deck` (deck, then hand), `put_in_hand`, `set_active`, `clear_bench`,
  `add_to_bench`, `attach_from_deck`, `end_turns`, plus `offers`, `find_action` and
  `cards_accounted_for` for checking results. Cards are only ever moved, never created, so every
  player always has exactly 60.
- **Fixtures** (in the engine tests): a struct holding the `CardDB`, `CardTable` and `GameState`
  in that order, because each points into the one before it. They're non-copyable on purpose.
- **`POKEBOT_SOURCE_DIR`**: a compile-time define with the repository path, so tests can read
  files like `data/decks/*.txt` no matter which folder they run from.

## Adding a test file

1. Create `tests/test_<name>.cpp` and include `<catch2/catch_test_macros.hpp>`.
2. Add it to `add_executable(pokebot_tests ...)` in [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt).
3. Give its tests a tag like `[name]` so the group can be run on its own.

`catch_discover_tests` registers each `TEST_CASE` with CTest automatically.

Keep square brackets in test names balanced, as in `[-1, 1]`, or leave them out. CMake reads the whole
test listing as one value and doesn't split it at a `;` inside brackets, so a lone `]`, as in `(0, 1]`,
lets the `;` in another test's name cut the listing in two, and discovery fails with a JSON
syntax error. The test executable then gets deleted, and a stale test list can hide new tests.
