theory CIM_V0a_Log
  imports Main
begin

type_synonym seq = nat
type_synonym key = nat
type_synonym cell_value = int
type_synonym delta = int
type_synonym amap = "key => cell_value"

datatype event = Event seq key delta

fun event_seq :: "event => seq" where
  "event_seq (Event s _ _) = s"

fun apply_event :: "event => amap => amap" where
  "apply_event (Event _ k d) m = m(k := m k + d)"

fun replay :: "event list => amap => amap" where
  "replay [] m = m"
| "replay (e # es) m = replay es (apply_event e m)"

fun valid_from :: "seq => event list => bool" where
  "valid_from n [] = True"
| "valid_from n (Event s _ _ # es) = (s = n & valid_from (Suc n) es)"

definition valid_log :: "event list => bool" where
  "valid_log es = valid_from 0 es"

definition snapshot :: "event list => amap => amap" where
  "snapshot prefix m = replay prefix m"

lemma replay_deterministic:
  assumes "events = events'" and "m = m'"
  shows "replay events m = replay events' m'"
  using assms by simp

lemma replay_append:
  "replay (xs @ ys) m = replay ys (replay xs m)"
  by (induction xs arbitrary: m) simp_all

lemma replay_append_single:
  "replay (xs @ [e]) m = apply_event e (replay xs m)"
  using replay_append by simp

lemma snapshot_tail_equiv:
  "replay (prefix @ tail) m = replay tail (snapshot prefix m)"
  unfolding snapshot_def using replay_append by simp

lemma valid_from_append:
  "valid_from n (xs @ ys) = (valid_from n xs & valid_from (n + length xs) ys)"
proof (induction xs arbitrary: n)
  case Nil
  then show ?case by simp
next
  case (Cons e xs)
  then show ?case by (cases e) simp
qed

lemma valid_log_prefix_tail:
  assumes "valid_log (prefix @ tail)"
  shows "valid_log prefix" and "valid_from (length prefix) tail"
  using assms unfolding valid_log_def
  by (auto simp: valid_from_append)

lemma snapshot_tail_equiv_valid:
  assumes "valid_log (prefix @ tail)"
  shows "valid_log prefix"
    and "valid_from (length prefix) tail"
    and "replay (prefix @ tail) m = replay tail (snapshot prefix m)"
  using assms valid_log_prefix_tail snapshot_tail_equiv by blast+

end
