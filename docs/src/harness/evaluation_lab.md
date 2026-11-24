# `src/harness/evaluation_lab` — how well the evaluation predicts results

**Files:** [`src/harness/evaluation_lab.hpp`](../../../src/harness/evaluation_lab.hpp), [`src/harness/evaluation_lab.cpp`](../../../src/harness/evaluation_lab.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Scores weights for the [evaluation](../agents/evaluation/evaluation.md) on the results of saved
[self-play games](self_play.md), always on seeds the weights weren't fitted on.

```cpp
const SampleSet set = load_samples(db, {"replays/self-play/part1.jsonl"}, 16);
const std::vector<Model> models{fixed_model("hand", default_weights()), scaled_model("hand, scaled", default_weights())};
for (const ModelScore& score : score_models(set, models, 1)) {
    score.all.loss;   // held-out log loss
    score.change;     // against models[1], position by position
}
```

## Predictions and loss

e = tanh(z) with z = Σθφ, and tanh(z) = 2σ(2z) − 1, so e predicts a win with chance σ(2z)
(`win_chance`). `log_loss(z, result)` is the log loss of that chance, computed from z so that it
doesn't overflow; a draw counts as half a win. Predicting 50% everywhere scores ln 2 = 0.6931.

## Models

A `Model` is a name and a function that gives weights from training samples. `fixed_model` ignores the
samples. `scaled_model` multiplies its weights by the one scale from 0 to 10 that fits the samples
best; the loss is convex in the scale, so `best_scale` halves the range on the slope. Weights that
predict backwards get 0.

## Scoring

- **Folds:** 10, by `seed % 10`. Each fold is scored by weights fitted on the other nine, so a seed's
  games are never scored by weights fitted on them. `weights` in the score are fitted on every seed,
  and aren't the ones scored.
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

`--evaluate FILE,FILE,...` scores a coin flip, the prizes weight with a fitted scale, the hand θ as
played, and the hand θ with a fitted scale (the reference). M10's part 5 is kept aside for the final
weights, so it isn't passed here until then.

## Tests

[`tests/test_evaluation_lab.cpp`](../../../tests/test_evaluation_lab.cpp), tag `[evaluation_lab]`:

- log loss and win chance, at known and extreme values;
- a known scale recovered, and 0 for weights that predict backwards;
- doubling every sample within its seed changes neither the loss nor its error;
- no seed scored by a model fitted on it;
- every breakdown covers all positions, and the reference doesn't change from itself;
- samples the same on any number of threads, and a repeated seed rejected.
