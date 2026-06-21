theory CIM_V0b_CheckedReplay
  imports CIM_V0a_Log
begin

datatype event_kind =
    Pairwise
  | Unsupported

datatype checked_event = CheckedEvent seq event_kind key delta

datatype replay_error =
    BadSequence
  | UnsupportedType
  | CapacityExceeded

datatype checked_result =
    ReplayOk amap
  | ReplayError replay_error

fun checked_apply :: "checked_event => amap => amap" where
  "checked_apply (CheckedEvent _ _ k d) m = m(k := m k + d)"

fun remember_key :: "key => key list => key list" where
  "remember_key k seen = (if k \<in> set seen then seen else k # seen)"

fun checked_replay_from ::
  "seq => seq => nat => key list => checked_event list => amap => checked_result"
where
  "checked_replay_from max_seq expected cap seen [] m = ReplayOk m"
| "checked_replay_from max_seq expected cap seen (CheckedEvent s kind k d # es) m =
    (if s ~= expected | s = max_seq then ReplayError BadSequence
     else if kind ~= Pairwise then ReplayError UnsupportedType
     else if k \<notin> set seen & length seen >= cap then ReplayError CapacityExceeded
     else checked_replay_from max_seq (Suc expected) cap (remember_key k seen) es
            (checked_apply (CheckedEvent s kind k d) m))"

definition checked_replay :: "seq => nat => checked_event list => amap => checked_result" where
  "checked_replay max_seq cap es m = checked_replay_from max_seq 0 cap [] es m"

lemma checked_replay_empty:
  "checked_replay max_seq cap [] m = ReplayOk m"
  unfolding checked_replay_def by simp

lemma bad_sequence_rejected:
  assumes "s ~= expected"
  shows "checked_replay_from max_seq expected cap seen
           (CheckedEvent s kind k d # es) m = ReplayError BadSequence"
  using assms by simp

lemma max_sequence_rejected:
  "checked_replay_from max_seq max_seq cap seen
     (CheckedEvent max_seq kind k d # es) m = ReplayError BadSequence"
  by simp

lemma first_sequence_must_be_zero:
  assumes "s ~= 0"
  shows "checked_replay max_seq cap (CheckedEvent s kind k d # es) m =
         ReplayError BadSequence"
  using assms unfolding checked_replay_def by simp

lemma unsupported_type_rejected:
  assumes "expected ~= max_seq"
  shows "checked_replay_from max_seq expected cap seen
           (CheckedEvent expected Unsupported k d # es) m =
         ReplayError UnsupportedType"
  using assms by simp

lemma capacity_exceeded_rejected:
  assumes "expected ~= max_seq"
    and "k \<notin> set seen"
    and "length seen >= cap"
  shows "checked_replay_from max_seq expected cap seen
           (CheckedEvent expected Pairwise k d # es) m =
         ReplayError CapacityExceeded"
  using assms by simp

lemma bad_sequence_not_ok:
  assumes "s ~= expected"
  shows "checked_replay_from max_seq expected cap seen
           (CheckedEvent s kind k d # es) m ~= ReplayOk m'"
  using assms by simp

lemma unsupported_type_not_ok:
  assumes "expected ~= max_seq"
  shows "checked_replay_from max_seq expected cap seen
           (CheckedEvent expected Unsupported k d # es) m ~= ReplayOk m'"
  using assms by simp

lemma capacity_exceeded_not_ok:
  assumes "expected ~= max_seq"
    and "k \<notin> set seen"
    and "length seen >= cap"
  shows "checked_replay_from max_seq expected cap seen
           (CheckedEvent expected Pairwise k d # es) m ~= ReplayOk m'"
  using assms by simp

end
