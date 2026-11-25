# `src/harness/weight_fit` — fitting the evaluation's weights

**Files:** [`src/harness/weight_fit.hpp`](../../../src/harness/weight_fit.hpp), [`src/harness/weight_fit.cpp`](../../../src/harness/weight_fit.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Fits the [evaluation](../agents/evaluation/evaluation.md)'s weights to the results of
[self-play positions](self_play.md), by logistic regression. The [evaluation lab](evaluation_lab.md)
scores the fits.

```cpp
FitOptions options;
options.non_negative = true;
const Fit fit = fit_weights(train, decks, options);   // train: pointers to Samples
save_weights("data/weights/m10/fitted.txt", fit.weights);
```

## The model

The chance that the player to move wins is σ(2z), with

> z = θ·φ + first + (turn start or later) + pairing

- **θ·φ** is the evaluation: e = tanh(θ·φ), and tanh(z) = 2σ(2z) − 1.
- **first:** +1 times its term if the player to move went first, −1 times it if not.
- **turn start, later:** a term for the player to move at a turn's first main-phase decision, and one
  for later in the turn.
- **pairing:** a term for each pair of decks, with the sign of whichever deck the player to move has. A
  mirror match has none.

Only θ is played with. The terms are there so that θ doesn't bend to explain which deck is stronger, or
whose turn it is. Without them, a feature that only marks the deck gets a weight.

`Sample` is a [`Position`](self_play.md) with its game's seed and decks (as numbers). `GameTerms` holds
the terms in the order first, turn start, later, pairings; `pairing_term(a, b, decks)` with a < b gives
a pairing's place.

## The fits

- **`fit_weights`** fits θ and the terms.
- **`fit_scale`** fits one number that multiplies given weights, and the terms.
- **`fit_terms`** fits the terms with θ held fixed.

`FitOptions`: `l2` adds l2/2 · Σθ² to the summed loss (so 1,000 is like 1,000 positions' worth of
curvature at most), `non_negative` keeps θ ≥ 0 (or the scale ≥ 0), `game_terms` false leaves the
terms out, and `use` says which features `fit_weights` fits: the others weigh 0
(`first_features(n)` is the first n, and the default is all of them). The terms have a very small L2 of their own, so that one without samples is 0.

## The solver

Newton's method. Each step builds the gradient and the Hessian in one pass over the samples, and
minimises that quadratic by coordinate descent, in which θ ≥ 0 is a clamp on each coordinate, until a
sweep gains less than 1e-16 per sample. The step
is halved until the objective doesn't rise. It stops when the quadratic predicts a gain below 1e-12 per
sample (`converged`), or after 50 steps. The problem is convex, so there's one minimum to find.

Rows are built as they're needed, so a fit takes no memory beyond the samples.

## Tests

[`tests/test_weight_fit.cpp`](../../../tests/test_weight_fit.cpp), tag `[weight_fit]`:

- known weights and terms recovered from exact chances, and from random wins and losses;
- a non-negative fit has no negative weight, and no small change improves it;
- a feature the fit doesn't use weighs 0, and the others are recovered;
- more L2 gives smaller weights;
- the pairing terms stop a feature that only marks the deck from getting a weight;
- fitting the terms leaves the weights alone.
