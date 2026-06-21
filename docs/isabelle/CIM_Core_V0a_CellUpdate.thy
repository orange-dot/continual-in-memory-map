theory CIM_Core_V0a_CellUpdate
  imports Complex_Main
begin

text \<open>
  Core proof lane for the CINM adaptive cell (decision D011 core line).

  This models ONE cell update as abstract scalar arithmetic over the reals --
  it is deliberately NOT a model of the C float code. Floating-point behaviour
  remains a known proof gap (see docs/FORMAL_PROOF.md). The log/replay sidecar
  proofs live separately in CIM_V0a_Log / CIM_V0b_CheckedReplay.
\<close>

type_synonym scalar = real

definition clamp :: "scalar => scalar => scalar => scalar" where
  "clamp lo hi x = (if x < lo then lo else if x > hi then hi else x)"

definition cell_update ::
  "scalar => scalar => scalar => scalar => scalar => scalar => scalar" where
  "cell_update wmax eta plast reward dphi w =
     clamp (-wmax) wmax (w + eta * plast * reward * dphi)"

definition raw_step :: "scalar => scalar => scalar => scalar => scalar" where
  "raw_step eta plast reward dphi = eta * plast * reward * dphi"

definition decay :: "scalar => scalar => scalar" where
  "decay f w = f * w"

definition margin :: "scalar => scalar => scalar" where
  "margin w dphi = w * dphi"


subsection \<open>Clamp facts\<close>

lemma clamp_lower:
  assumes "lo <= hi"
  shows "lo <= clamp lo hi x"
  using assms unfolding clamp_def by auto

lemma clamp_upper:
  assumes "lo <= hi"
  shows "clamp lo hi x <= hi"
  using assms unfolding clamp_def by auto

lemma clamp_mono:
  assumes "lo <= hi" and "x <= y"
  shows "clamp lo hi x <= clamp lo hi y"
  using assms unfolding clamp_def by auto

lemma clamp_id:
  assumes "lo <= x" and "x <= hi"
  shows "clamp lo hi x = x"
  using assms unfolding clamp_def by auto


subsection \<open>Core obligations\<close>

text \<open>(a) A bounded update never leaves [-wmax, wmax].\<close>
lemma cell_update_bounded:
  assumes "0 <= wmax"
  shows "-wmax <= cell_update wmax eta plast reward dphi w
       & cell_update wmax eta plast reward dphi w <= wmax"
proof -
  from assms have le: "- wmax <= wmax" by linarith
  have "-wmax <= cell_update wmax eta plast reward dphi w"
    unfolding cell_update_def by (rule clamp_lower[OF le])
  moreover have "cell_update wmax eta plast reward dphi w <= wmax"
    unfolding cell_update_def by (rule clamp_upper[OF le])
  ultimately show ?thesis by simp
qed

text \<open>(b) Lower plasticity commands no larger a step than higher plasticity.
  Stated on the UNCLAMPED step magnitude; saturation is covered by (a). The
  naive applied-delta form is false once the clamp saturates.\<close>
lemma plasticity_step_mono:
  assumes "0 <= p1" and "p1 <= p2"
  shows "abs (raw_step eta p1 reward dphi) <= abs (raw_step eta p2 reward dphi)"
proof -
  have p2nn: "0 <= p2" using assms by linarith
  have factor: "abs (raw_step eta p reward dphi) = abs p * abs (eta * reward * dphi)" for p
    unfolding raw_step_def by (simp add: abs_mult mult_ac)
  have "abs (raw_step eta p1 reward dphi) = p1 * abs (eta * reward * dphi)"
    using factor[of p1] \<open>0 <= p1\<close> by simp
  also have "... <= p2 * abs (eta * reward * dphi)"
    by (rule mult_right_mono[OF \<open>p1 <= p2\<close> abs_ge_zero])
  also have "... = abs (raw_step eta p2 reward dphi)"
    using factor[of p2] p2nn by simp
  finally show ?thesis .
qed

text \<open>(c) For a positive feature delta, negative reward does not raise the margin,
  given the standing invariant that the stored weight is already in [-wmax, wmax]
  (every stored weight is the clamped output of a prior update; init is 0).\<close>
lemma negative_reward_margin_noninc:
  assumes "0 < dphi" and "reward < 0" and "0 < eta" and "0 < plast"
      and "-wmax <= w" and "w <= wmax"
  shows "margin (cell_update wmax eta plast reward dphi w) dphi <= margin w dphi"
proof -
  have lehi: "-wmax <= wmax" using assms by linarith
  have step_neg: "eta * plast * reward * dphi <= 0"
  proof -
    have ep: "0 < eta * plast" by (rule mult_pos_pos[OF \<open>0 < eta\<close> \<open>0 < plast\<close>])
    have epr: "eta * plast * reward <= 0"
      by (rule mult_nonneg_nonpos[OF less_imp_le[OF ep] less_imp_le[OF \<open>reward < 0\<close>]])
    show ?thesis
      by (rule mult_nonpos_nonneg[OF epr less_imp_le[OF \<open>0 < dphi\<close>]])
  qed
  have cu_le: "cell_update wmax eta plast reward dphi w <= w"
  proof -
    have wle: "w + eta * plast * reward * dphi <= w" using step_neg by linarith
    have "cell_update wmax eta plast reward dphi w <= clamp (-wmax) wmax w"
      unfolding cell_update_def by (rule clamp_mono[OF lehi wle])
    also have "clamp (-wmax) wmax w = w" by (rule clamp_id[OF \<open>-wmax <= w\<close> \<open>w <= wmax\<close>])
    finally show ?thesis .
  qed
  show ?thesis unfolding margin_def
    by (rule mult_right_mono[OF cu_le less_imp_le[OF \<open>0 < dphi\<close>]])
qed

text \<open>(d) Decay by a factor in [0,1] never increases the absolute weight.\<close>
lemma decay_noninc:
  assumes "0 <= f" and "f <= 1"
  shows "abs (decay f w) <= abs w"
proof -
  have "abs (decay f w) = abs f * abs w" unfolding decay_def by (rule abs_mult)
  also have "... = f * abs w" using \<open>0 <= f\<close> by simp
  also have "... <= 1 * abs w" by (rule mult_right_mono[OF \<open>f <= 1\<close> abs_ge_zero])
  also have "... = abs w" by simp
  finally show ?thesis .
qed

end
