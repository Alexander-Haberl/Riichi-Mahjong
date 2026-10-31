# Yaku specification (first set)

Implementation notes for the first 21 yaku of this project, all implemented in `src/rules/Yaku.cpp`. Kokushi Musou is implemented there too, as the first yakuman (`form == Kokushi`, 13 han; the double version for the 13-sided wait comes later). Remaining yaku (junchan, honroutou, shousangen, sanshoku doukou, sankantsu, the other yakuman) come later and are not covered here.

## How a yaku plugs in

A yaku is a `Yaku` value (`src/rules/Yaku.hpp`), a name plus a function, and is listed in the table in `standardYaku`:

```cpp
using HanFunction = std::function<int(const Reading&, const WinContext&)>;  // 0 = not present

struct Yaku {
    std::string_view name;
    HanFunction han;
};
```

`Scorer::score` calls every yaku once per `Reading` and adds up the results. It does not know about conflicts between yaku, so **each yaku's condition has to exclude the yaku it conflicts with** (see the conflict table below).

What the data means (`src/rules/Decomposer.hpp`, `src/rules/WinContext.hpp`):

- `reading.form`: `Regular`, `Chiitoitsu` or `Kokushi`.
- `reading.blocks`: concealed blocks **and called melds**, so a yaku sees the whole hand.
  - Regular: one `Pair`, four sets of type `Sequence`, `Triplet` or `Kan`.
  - Chiitoitsu: seven `Pair` blocks.
  - Kokushi: **empty**. `std::all_of` on empty blocks is true, so every block-based yaku must check the form or it fires on kokushi.
- `block.first`: lowest tile of the block, never red. A sequence is `first`, `first + 1`, `first + 2`.
- `block.open`: the block counts as open. True for called melds **and** for a triplet completed by ron in this reading. False for closed kans.
- `reading.handOpen`: the hand has an open meld. A closed kan keeps it false, and a ron-completed triplet does not change it. **Use this for "closed only" and for reduced han**, not `block.open`.
- `reading.wait`: the wait the winning tile completed (`Ryanmen`, `Kanchan`, `Penchan`, `Shanpon`, `Tanki`). Always set when called from the scorer.
- `ctx`: the `WinContext` flags (`tsumo`, `riichi`, `doubleRiichi`, `ippatsu`, `haitei`, `houtei`, `rinshan`, `chankan`) and `seatWind`, `roundWind`.

A `Kan` counts as a triplet for every yaku below. Red fives have no effect on any yaku. Dora are not yaku; the scorer handles them.

## The yaku

"Open" is the han with `reading.handOpen == true`. A dash means the yaku does not exist on an open hand: return 0.

### Situation yaku (no look at the blocks)

| Name | Closed | Open | Condition |
|---|---|---|---|
| Riichi | 1 | – | `ctx.riichi && !ctx.doubleRiichi` |
| Double Riichi | 2 | – | `ctx.doubleRiichi`. `declareRiichi` sets both flags, so riichi has to exclude it. |
| Ippatsu | 1 | – | `ctx.ippatsu`. The controller clears it on a call or the player's next discard. |
| Menzen Tsumo | 1 | – | `ctx.tsumo && !reading.handOpen` |
| Haitei | 1 | 1 | `ctx.haitei`: tsumo on the last live-wall tile, never on a rinshan tile |
| Houtei | 1 | 1 | `ctx.houtei`: ron on the last discard |
| Rinshan Kaihou | 1 | 1 | `ctx.rinshan`: tsumo on the replacement tile after a kan |
| Chankan | 1 | 1 | `ctx.chankan`: ron on a tile added to a pon |

The context flags are already correct for tsumo and ron, so no extra `ctx.tsumo` check is needed for haitei and rinshan.

### Hand-shape yaku

| Name | Closed | Open | Forms | Condition |
|---|---|---|---|---|
| Tanyao | 1 | 1 (rule) | Regular, Chiitoitsu | No terminals or honors in any block. Sequence: `first.number()` is 2 to 6 (and not an honor). Other blocks: `first.isSimple()`. |
| Pinfu | 1 | – | Regular | All four sets are `Sequence`; the pair is not a dragon, seat wind or round wind; `reading.wait == Ryanmen`. |
| Yakuhai | 1 each | 1 each | Regular | See below. |
| Iipeikou | 1 | – | Regular | `identicalSequencePairs(reading) == 1` |
| Ryanpeikou | 3 | – | Regular | `identicalSequencePairs(reading) == 2` |
| Chiitoitsu | 2 | – | Chiitoitsu | `reading.form == Chiitoitsu`. The decomposer guarantees seven different pairs. |
| Toitoi | 2 | 2 | Regular | No `Sequence` block. |
| Sanankou | 2 | 2 | Regular | At least 3 `Triplet` / `Kan` blocks with `!block.open`. The hand may have open melds. |
| Sanshoku | 2 | 1 | Regular | Sequences with the same `first.number()` in man, pin and sou. |
| Ittsu | 2 | 1 | Regular | Sequences starting at 1, 4 and 7 in one suit. |
| Chanta | 2 | 1 | Regular | Every block contains a terminal or honor (sequence: `first.number()` is 1 or 7; other blocks: `first.isTerminalOrHonor()`), **and** at least one sequence, **and** at least one honor block. |
| Honitsu | 3 | 2 | Regular, Chiitoitsu | All suited blocks are in one suit, and at least one honor block exists. |
| Chinitsu | 6 | 5 | Regular, Chiitoitsu | All blocks are in one suit, no honor block. |

Details:

- **Tanyao and the rule.** `RuleSet::kuitan` (default true): tanyao also counts on an open hand. The han function does not receive the rules, so `tanyao(rules.kuitan)` returns a function that captures the flag. With `kuitan == false`, an open hand returns 0.
- **Yakuhai.** Five separate `Yaku` entries, so each shows up on its own in `ScoreResult::hanSources`: White, Green, Red, Seat Wind, Round Wind. Each returns 1 if the reading has a `Triplet` or `Kan` of that tile. A pair does not count. For the winds compare with `Tile::honor(ctx.seatWind)` / `Tile::honor(ctx.roundWind)`. A wind that is both seat and round wind therefore gives 2 han without special handling.
- **`identicalSequencePairs(reading)`.** A shared helper. For each `first` among the `Sequence` blocks, take `count / 2`, and sum. Three identical sequences give 1, four give 2.
- **Pinfu.** Same shape as the "closed hand without fu" case in `unroundedFu` (`src/rules/Scorer.cpp`), but the yaku needs its own check, including `wait == Ryanmen`.
- **Sanankou.** Relies on `block.open`: on a shanpon ron, that triplet is open, so three closed-looking triplets can drop to two. Exactly this case must be tested. On tsumo the triplet stays closed. Closed kans count; open kans do not. Use `>= 3`: suuankou will replace it later as a yakuman.
- **Chanta needs an honor.** A hand with sequences and terminals but no honors is junchan, which is a later yaku. Until then such a hand gets neither yaku; that is intended.
- **Honitsu / chinitsu / tanyao on chiitoitsu.** These work by iterating over the seven pairs, and combine with chiitoitsu.

## Conflicts between yaku

All of these are handled by the conditions above. No yaku may look at another yaku.

| Pair | Separated by |
|---|---|
| Riichi / Double Riichi | `!ctx.doubleRiichi` in riichi |
| Iipeikou / Ryanpeikou | 1 or 2 identical sequence pairs |
| Honitsu / Chinitsu | honitsu needs an honor, chinitsu has none |
| Chanta / (later) Junchan | chanta needs an honor |
| Chanta / (later) Honroutou | chanta needs a sequence |
| Chiitoitsu / Ryanpeikou | different readings; the scorer keeps the better one |

Things that do **not** conflict: chiitoitsu with tanyao, honitsu or chinitsu; toitoi with sanankou; yakuhai with everything but chiitoitsu.

## Example hands for tests

Tile strings use the project's notation (`parseTiles`). Check each hand's readings when using it: the scorer tries all of them. Win tile in brackets where it matters.

| Yaku | Hand with the yaku | Case without it |
|---|---|---|
| Tanyao | `234m345p456s678s22m` | `123m345p456s678s22m`: contains 1m |
| Pinfu | `123m456s789s234p55s` [2p] | same hand [3p]: kanchan; pair `55z` instead: dragon pair |
| Yakuhai | `123m456p789s55s777z`: Red | `123m456p789s111z77z`: 77z is only a pair (and East counts with East seat or round) |
| Iipeikou | `112233m456p789s55z` | same shape open: 0; `112233m445566p77z`: ryanpeikou instead |
| Ryanpeikou | `112233m445566p77z` | also has a chiitoitsu reading: the scorer must pick ryanpeikou |
| Chiitoitsu | `1133557799m1155p` | `1111m2233p4455s66z`: four of a kind is not two pairs |
| Toitoi | `111m222p333s444s55z` | any sequence |
| Sanankou | `111m222p333s456s55z` [6s] | same hand, ron [1m]: 111m is open, only 2 closed triplets. Tsumo [1m] keeps it. |
| Sanshoku | `123m123p123s789s55z` | `123m123p234s789s55z`: numbers differ |
| Ittsu | `123456789m123p55z` | `123456789m` split across suits |
| Chanta | `123m789p789s111z55z` | no honor: junchan; no sequence: honroutou |
| Honitsu | `111m234m567m999m22z` | `111m234m567m999m22m`: chinitsu |
| Chinitsu | `111m234m567m999m22m` | any honor or second suit |

## Implementation hints

- `standardYaku(rules)` is the table of all yaku, and `standardScorer(rules)` builds a `Scorer` from it. A new yaku is one function in `Yaku.cpp` plus one line in the table.
- Keep shared block helpers (`identicalSequencePairs`, "has a triplet or kan of this tile", "tiles of the block are terminal or honor") together with the yaku, not in `Decomposer`.
- Each yaku needs at least one positive and one negative test, plus the open-hand han where it differs. `tests/test_yaku.cpp` has a `han(name, ctx)` helper that runs one yaku over all readings, and `win()` in `tests/TestHelpers.hpp` builds hands with melds.
- Build with `LC_ALL=C` when checking for warnings (the compiler output is otherwise German), and run both `build` and `build-asan`.
