# Yaku specification

Implementation notes for all yaku of this project, in `src/scoring/Yaku.cpp`. All yaku and yakuman described here are implemented, including the double yakuman. Nagashi mangan is not a yaku but is implemented in the round controller (see below). Not implemented: renhou.

## How a yaku plugs in

A yaku is a `Yaku` value (`src/scoring/Yaku.hpp`), a name plus a function, and is listed in the table in `standardYaku`:

```cpp
using HanFunction = std::function<int(const Reading&, const WinContext&)>;  // 0 = not present

struct Yaku {
    std::string_view name;
    HanFunction han;
};
```

`Scorer::score` calls every yaku once per `Reading` and adds up the results. It does not know about conflicts between yaku, so **each yaku's condition has to exclude the yaku it conflicts with** (see the conflict table below).

What the data means (`src/core/Decomposer.hpp`, `src/scoring/WinContext.hpp`):

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
- **Pinfu.** Same shape as the "closed hand without fu" case in `unroundedFu` (`src/scoring/Scorer.cpp`), but the yaku needs its own check, including `wait == Ryanmen`.
- **Sanankou.** Relies on `block.open`: on a shanpon ron, that triplet is open, so three closed-looking triplets can drop to two. Exactly this case must be tested. On tsumo the triplet stays closed. Closed kans count; open kans do not. Use `>= 3`: suuankou will replace it later as a yakuman.
- **Chanta needs an honor.** A hand with sequences and terminals but no honors is junchan, which is a later yaku. Until then such a hand gets neither yaku; that is intended.
- **Honitsu / chinitsu / tanyao on chiitoitsu.** These work by iterating over the seven pairs, and combine with chiitoitsu.

### Remaining yaku

| Name | Closed | Open | Forms | Condition |
|---|---|---|---|---|
| Junchan | 3 | 2 | Regular | Every block contains a terminal (sequence: `first.number()` is 1 or 7; other blocks: `first.isTerminal()`), **and** at least one sequence. No honor block. |
| Honroutou | 2 | 2 | Regular, Chiitoitsu | Every block is a terminal or honor `Triplet` / `Kan` / `Pair` (`first.isTerminalOrHonor()`), no `Sequence`, **and** at least one terminal and at least one honor block. |
| Shousangen | 2 | 2 | Regular | Two dragon `Triplet` / `Kan` blocks and a dragon `Pair`. |
| Sanshoku Doukou | 2 | 2 | Regular | `Triplet` / `Kan` blocks with the same `first.number()` in man, pin and sou. |
| Sankantsu | 2 | 2 | Regular | Exactly 3 `Kan` blocks, open or closed. |

Details:

- **Junchan and chanta.** The same check as chanta with "terminal" instead of "terminal or honor", and without an honor block. Chanta already requires an honor, so the two never fire together.
- **Honroutou needs both kinds.** Without an honor block the hand is chinroutou, without a terminal block tsuuiisou: both yakuman, see below. Requiring both keeps honroutou off those hands. It is not strictly needed, since the scorer drops regular yaku once a yakuman is present, but it keeps the yaku correct on its own. Honroutou always comes with toitoi (regular form) or chiitoitsu; both add up.
- **Shousangen and yakuhai.** The two dragon triplets also count as yakuhai, so shousangen is 2 + 2 = 4 han. That is intended: do not exclude the dragons. With three dragon triplets the hand is daisangen (yakuman), and shousangen does not apply because the pair is not a dragon.
- **Sanshoku doukou.** Like sanshoku, but over triplets and kans instead of sequences. Honors do not take part.
- **Sankantsu.** Counts kans only, not their openness. Exactly 3, so suukantsu (yakuman) replaces it with 4.

## Conflicts between yaku

All of these are handled by the conditions above. No yaku may look at another yaku.

| Pair | Separated by |
|---|---|
| Riichi / Double Riichi | `!ctx.doubleRiichi` in riichi |
| Iipeikou / Ryanpeikou | 1 or 2 identical sequence pairs |
| Honitsu / Chinitsu | honitsu needs an honor, chinitsu has none |
| Chanta / Junchan | chanta needs an honor, junchan has none |
| Chanta / Honroutou | chanta needs a sequence, honroutou has none |
| Junchan / Honroutou | junchan needs a sequence |
| Sankantsu / Suukantsu | exactly 3 kans; suukantsu is a yakuman anyway |
| Chiitoitsu / Ryanpeikou | different readings; the scorer keeps the better one |

Things that do **not** conflict: chiitoitsu with tanyao, honitsu, chinitsu or honroutou; toitoi with sanankou, honroutou or sanshoku doukou; shousangen with the dragon yakuhai; junchan with chinitsu (e.g. `123m123m789m999m11m`); yakuhai with everything but chiitoitsu.

Regular yaku never need to exclude yakuman: as soon as a reading has a yakuman, the scorer keeps only the yakuman.

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
| Junchan | `123m789p789s123s99m` | `123m789p789s111z99m`: has an honor, chanta |
| Honroutou | `111m999p111s111z99s`; chiitoitsu `1199m1199p11s1122z` | `111m999p111s999s99m`: no honor, chinroutou |
| Shousangen | `555z666z77z123m456p` | `555z666z777z123m11p`: daisangen; `555z66z123m456p789s`: one dragon triplet |
| Sanshoku Doukou | `222m222p222s456s11z` | `222m222p333s456s11z`: numbers differ |
| Sankantsu | three kans (melds) + `456m11z` | two kans |

## Yakuman

A yakuman is an ordinary `Yaku` whose han function returns `kYakumanHan` (13), or `2 * kYakumanHan` for a double yakuman. The scorer already does the rest (`Scorer::score`):

- As soon as one yaku of a reading returns `>= kYakumanHan`, only the yakuman count; regular yaku and dora are dropped.
- `RuleSet::doubleYakuman`: off, every yakuman counts as 13 even if it returns 26.
- `RuleSet::stackYakuman`: off, only the highest yakuman of the reading counts; on, they add up.
- `basicPoints` turns the sum into 8000 basic points per 13 han.

So a double yakuman and its single version are **one** `Yaku` entry that returns 13 or 26, never two entries. Different yakuman (daisangen and tsuuiisou, say) are separate entries and stack when the rule allows it.

### The yakuman

| Name | Value | Forms | Closed only | Condition |
|---|---|---|---|---|
| Kokushi Musou | 13, 26 (13-sided wait) | Kokushi | yes (form) | `form == Kokushi`. Double when the hand without the winning tile holds all 13 kinds once (a 13-sided wait). |
| Suuankou | 13, 26 (tanki) | Regular | yes | 4 `Triplet` / `Kan` blocks with `!block.open`. Double when `reading.wait == Tanki`. A shanpon ron opens one triplet, so it is then only sanankou + toitoi. |
| Daisangen | 13 | Regular | no | `Triplet` / `Kan` of all three dragons. |
| Shousuushii | 13 | Regular | no | Three wind `Triplet` / `Kan` blocks and a wind `Pair`. |
| Daisuushii | 26 | Regular | no | `Triplet` / `Kan` of all four winds. |
| Tsuuiisou | 13 | Regular, Chiitoitsu | no | Every block is an honor block. |
| Chinroutou | 13 | Regular | no | Every block is a terminal `Triplet` / `Kan` / `Pair` (no honors, no sequences). |
| Ryuuiisou | 13 | Regular | no | Every tile is one of 2s 3s 4s 6s 8s and 6z (green dragon). Sequences can only be 234s. The green dragon is not required. |
| Chuuren Poutou | 13, 26 (9-sided wait) | Regular | yes | One suit, and the tile counts are 1112345678999 plus one extra tile of that suit. Double when the hand without the winning tile is exactly 1112345678999. |
| Suukantsu | 13 | Regular | no | 4 `Kan` blocks, open or closed. |
| Tenhou | 13 | any | yes | The dealer wins by tsumo on their first draw. |
| Chiihou | 13 | any | yes | A non-dealer wins by tsumo on their first draw, with no call (closed kan included) before it. |

Details:

- **Shape checks without blocks.** Kokushi's 13-sided wait and chuuren poutou look at tile counts, not blocks: take `ctx.hand.counts()` (the complete hand, winning tile included) and remove `ctx.winTile` to get the hand before the win. Chuuren needs no melds at all (`ctx.hand.meldCount() == 0`; a closed kan breaks the shape too).
- **Suuankou and the scorer.** Tanki suuankou has one reading with `wait == Tanki`; a shanpon reading of the same hand (if any) returns 13 or 0. The scorer keeps the best reading, so the double wins on its own.
- **Shousuushii / daisuushii.** Separated by the pair: shousuushii needs the wind pair, so with four wind triplets it cannot fire. Wind yakuhai do not matter, the yakuman replaces them.
- **Tsuuiisou on chiitoitsu.** Seven different honor pairs are possible (there are exactly seven honors), so chiitoitsu counts. Chinroutou and ryuuiisou cannot be chiitoitsu (only six terminal kinds, six green kinds), so they need no chiitoitsu branch.
- **Daisangen and shousangen**, **daisuushii and shousuushii**, **suukantsu and sankantsu**: the regular yaku of each pair is dropped by the scorer anyway; the yakuman need no exclusion against them.
- **Tenhou and chiihou need context.** `WinContext` has no first-draw flag yet. Add `bool firstDraw` and set it in `tsumoContext` (`src/game/LegalActions.cpp`) from `player.firstTurn() && !state.anyCall`; `anyCall` is also set by kans, so a closed kan ends the first go-around too. Tenhou: `ctx.firstDraw && ctx.seatWind == Honor::East`; chiihou: `ctx.firstDraw && ctx.seatWind != Honor::East`. A rinshan tile is not a first draw: it follows a kan, which sets `anyCall`.
- **Not planned:** renhou (ron before the first own draw) differs too much between rule sets; it can come later as a `RuleSet` option.

### Not a yaku: nagashi mangan

Nagashi mangan (all discards terminals and honors, none called, at an exhaustive draw) is no win: there is no winning hand to score. It lives in the round controller, not in `Yaku.cpp`, and is implemented that way: `RuleSet::nagashiMangan`, checked in `RoundController::finishExhaustiveDraw`, reported in `RoundResult::nagashi` and the `ExhaustiveDraw` event, and paid by `payments` like a mangan tsumo for each such player. It replaces the tenpai payments; honba are not paid and stay on the table with the riichi sticks; the dealer keeps the seat when tenpai or with their own nagashi mangan.

### Example hands for tests

| Yakuman | Hand with it | Case without it / double |
|---|---|---|
| Kokushi | `19m19p19s11234567z` [7z]: single wait (the 1z pair was there before) | same hand [1z]: 13-sided wait, double |
| Suuankou | `111m222p333s444s55z` tsumo [2p] | same hand ron [2p]: shanpon ron, only sanankou + toitoi; [5z]: tanki, double |
| Daisangen | `555z666z777z123m11p` | `555z666z77z123m456p`: shousangen |
| Shousuushii | `111z222z333z44z123m` | `111z222z333z444z11m`: daisuushii |
| Daisuushii | `111z222z333z444z11m` | three wind triplets only |
| Tsuuiisou | `111z222z555z666z77z`; chiitoitsu `11223344556677z` | any number tile |
| Chinroutou | `111m999m111p999s11s` | an honor: honroutou |
| Ryuuiisou | `234s234s666s888s66z` | `234s234s666s888s55z`: white dragon |
| Chuuren Poutou | `11123455678999m` [9m] | same hand [5m]: 9-sided wait (1112345678999 before), double |
| Suukantsu | four kans (melds) + `1z` [1z] | three kans: sankantsu |
| Tenhou | dealer, first draw tsumo | after a closed kan |
| Chiihou | non-dealer, first draw tsumo | after any call before |

## Implementation hints

- `standardYaku(rules)` is the table of all yaku, and `standardScorer(rules)` builds a `Scorer` from it. A new yaku is one function in `Yaku.cpp` plus one line in the table.
- Keep shared block helpers (`identicalSequencePairs`, "has a triplet or kan of this tile", "tiles of the block are terminal or honor") together with the yaku, not in `Decomposer`.
- Each yaku needs at least one positive and one negative test, plus the open-hand han where it differs. `tests/test_yaku.cpp` has a `han(name, ctx)` helper that runs one yaku over all readings, and `win()` in `tests/TestHelpers.hpp` builds hands with melds.
- Build with `LC_ALL=C` when checking for warnings (the compiler output is otherwise German), and run both `build` and `build-asan`.
