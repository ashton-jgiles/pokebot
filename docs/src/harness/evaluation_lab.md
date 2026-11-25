# `src/harness/evaluation_lab` — how well the evaluation predicts results

**Files:** [`src/harness/evaluation_lab.hpp`](../../../src/harness/evaluation_lab.hpp), [`src/harness/evaluation_lab.cpp`](../../../src/harness/evaluation_lab.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Scores weights for the [evaluation](../agents/evaluation/evaluation.md) on the results of saved
[self-play games](self_play.md), always on seeds the weights weren't fitted on.

```cpp
const SampleSet set = load_samples(db, {"replays/self-play/part1.jsonl"}, 16);
const std::size_t decks = set.decks.size();
const std::vector<Model> models{scaled_model("hand", default_weights(), decks), fitted_model("fitted", decks)};
for (const ModelScore& score : score_models(set, models, 0, 16)) {
    score.raw.total.loss;         // held-out log loss
    score.adjusted.total.change;  // deck-adjusted, against models[0], position by position
}
```

## Predictions and loss

e = tanh(z) with z = Σθφ, and tanh(z) = 2σ(2z) − 1, so e predicts a win with chance σ(2z)
(`win_chance`). `log_loss(z, result)` is the log loss of that chance, computed from z so that it
doesn't overflow; a draw counts as half a win. Predicting 50% everywhere scores ln 2 = 0.6931.

## Models

A `Model` is a name and a function that gives weights from training samples. `fixed_model` ignores the
samples, `scaled_model` multiplies its weights by the one number, at least 0, that fits best, and
`fitted_model` fits every weight. The last two use [`weight_fit`](weight_fit.md). Fits run on several
threads at once, so a model's function must allow that.

## Scoring

- **Folds:** 10, by `seed % 10`. Each fold is scored by weights fitted on the other nine, so a seed's
  games are never scored by weights fitted on them. `weights` in the score are fitted on every seed,
  and aren't the ones scored.
- **Raw and adjusted:** the raw loss is that of θ·φ alone. Raw loss rewards using features to recognise
  the deck, which is no use in a game, where the pairing never changes. So each fold also holds θ fixed,
  fits the [terms](weight_fit.md) for the pairing, going first and the player to move on its training
  seeds, and scores with them: the adjusted loss, by which weights are chosen.
- **Left-out decks** (with three decks or more): θ is fitted without any game of a deck, then scored,
  adjusted, on that deck's games in the held-out seeds. The terms are fitted on all decks, so this tests
  θ and not a guess at the deck's strength. `left_out` is in the order of `decks`.
- **`weight_errors`** are from the ten folds' fits (a jackknife).
- **Errors are clustered by seed:** a game has about 39 positions that share one result, so the spread
  is taken between seeds. The `_error` values are one standard error.
- **Change:** each model's loss less the reference's on the same position, so the noise they share
  cancels.
- **Breakdowns:** turn start or later in the turn, the smaller prize count, the mover's deck, and the
  predicted chance in ten bins (calibration). `gap_error` is the error of result less prediction.
- **Every position counts equally,** so long games count for more.

`load_samples` plays the games back on `threads` threads. A seed in two files is rejected, since its
games would be counted twice. The order of the samples doesn't depend on the number of threads.

## Limits

- The positions are ones that agents reached in their games. The search scores leaves, which are
  different positions, so a lower loss doesn't guarantee stronger play: the arena decides that.
- The games come from one kind of agent on four decks, and a result depends on the decks as well as
  the position, which e doesn't know.

## In the sim

`--evaluate FILE,FILE,...` scores these models, and `--save-weights DIR` saves the fitted ones.
`--reference NAME` compares them with another of the models, so that two close ones can be compared
position by position:

| Model | Weights |
|---|---|
| `coin` | all 0 |
| `prizes-scaled` | the prizes weight with a fitted scale |
| `hand` | the hand θ as played |
| `hand-scaled` | the hand θ with a scale fitted without terms |
| `hand-scaled-terms` | the hand θ with a scale fitted with terms: the default reference |
| `fitted`, `fitted-nonneg` | every weight fitted with terms, free and with θ ≥ 0 |
| `fitted-l2-1000`, `fitted-l2-10000` | free, with L2 |
| `fitted-no-terms` | free, fitted without terms |

It prints the raw and adjusted losses, the weights with their errors, the terms, the left-out decks, the
adjusted loss by prizes, and calibration for the reference and the two main fits. The results are in the
[roadmap](../../roadmap.md) under M10. M10's part 5 is kept aside for the final weights, so it isn't
passed here until then. 12,800 games take 24.7 s on 16 threads, 2.8 s of it to play them back.

### Candidate features

`--evaluate FILE,... --candidates` fits other models, all with terms and θ ≥ 0: the played features
(`without`, the reference), those with each [candidate](../agents/evaluation/evaluation.md#candidates)
added, and with all of them. `--with NAME,...` gives every model those candidates, so the next one can be
measured on top of them. It prints the adjusted loss and its change, the added weight, the change on each
left-out deck, the loss by prizes, and the weights. `--save-weights DIR` saves only the `without` model, as
`fitted-NAME-NAME.txt` from `--with`, which it needs.

Parts 1 to 4 take 30.9 s on 16 threads with all six candidates. The models of plain `--evaluate` use the
played features only.

## Tests

[`tests/test_evaluation_lab.cpp`](../../../tests/test_evaluation_lab.cpp), tag `[evaluation_lab]`:

- log loss and win chance, at known and extreme values;
- a known scale recovered, with and without terms, and 0 for weights that predict backwards;
- doubling every sample within its seed changes neither the loss nor its error;
- no seed scored by a model fitted on it;
- every breakdown covers all positions, and the reference doesn't change from itself;
- a left-out deck never reaches the fit;
- where only the pairing matters, the adjusted loss is below the raw one;
- samples and scores the same on any number of threads, and a repeated seed rejected.
