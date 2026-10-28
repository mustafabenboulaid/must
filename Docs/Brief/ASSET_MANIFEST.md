# Asset manifest — what exists in the user's Unreal project (2026-10-28)

The model never opens these files (`.uasset` are binary). They live in the user's project at `/Game/...`; the code loads them by these paths (`LoadObject<UAnimSequence>(nullptr, TEXT("<path>.<name>"))`) or exposes them as `UPROPERTY` / data-asset fields the user fills in the editor.

Skeleton: UE5 Manny / Quinn (`SK_Mannequin`). Main bones used by the code: root, pelvis, spine_01..05, neck_01, head, clavicle_l/r, upperarm_l/r, lowerarm_l/r, hand_l/r, thumb/index/middle/ring/pinky_01..03_l/r, thigh_l/r, calf_l/r, foot_l/r, ball_l/r, twist bones (thigh_twist_01, calf_twist_01, upperarm_twist_01, lowerarm_twist_01), ik_hand_*, ik_foot_*. Sockets used: weapon_r.

If the model needs a clip that is not listed here (a frisk, a salute, a prone crawl …), it must ASK the user to import it (e.g. from Mixamo) and give the path, or bake it into a header as done for the Mixamo clips (`AAAClip*.h`).


## `/Game/Characters/Mannequins/Anims/Death`

- `/Game/Characters/Mannequins/Anims/Death/MM_Death_Back_01.MM_Death_Back_01`
- `/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01.MM_Death_Front_01`
- `/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_02.MM_Death_Front_02`
- `/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_03.MM_Death_Front_03`
- `/Game/Characters/Mannequins/Anims/Death/MM_Death_Left_01.MM_Death_Left_01`
- `/Game/Characters/Mannequins/Anims/Death/MM_Death_Right_01.MM_Death_Right_01`

## `/Game/Characters/Mannequins/Anims/Pistol/Aim`

- `/Game/Characters/Mannequins/Anims/Pistol/Aim/AO_Pistol.AO_Pistol`
- `/Game/Characters/Mannequins/Anims/Pistol/Aim/MF_Pistol_Idle_ADS1.MF_Pistol_Idle_ADS1`
- `/Game/Characters/Mannequins/Anims/Pistol/Aim/MF_Pistol_Idle_ADS_AO_CD.MF_Pistol_Idle_ADS_AO_CD`
- `/Game/Characters/Mannequins/Anims/Pistol/Aim/MF_Pistol_Idle_ADS_AO_CU.MF_Pistol_Idle_ADS_AO_CU`

## `/Game/Characters/Mannequins/Anims/Pistol/Jog`

- `/Game/Characters/Mannequins/Anims/Pistol/Jog/MF_Pistol_Jog_Bwd.MF_Pistol_Jog_Bwd`
- `/Game/Characters/Mannequins/Anims/Pistol/Jog/MF_Pistol_Jog_Bwd_Left.MF_Pistol_Jog_Bwd_Left`
- `/Game/Characters/Mannequins/Anims/Pistol/Jog/MF_Pistol_Jog_Bwd_Right.MF_Pistol_Jog_Bwd_Right`
- `/Game/Characters/Mannequins/Anims/Pistol/Jog/MF_Pistol_Jog_Fwd.MF_Pistol_Jog_Fwd`
- `/Game/Characters/Mannequins/Anims/Pistol/Jog/MF_Pistol_Jog_Fwd_Left.MF_Pistol_Jog_Fwd_Left`
- `/Game/Characters/Mannequins/Anims/Pistol/Jog/MF_Pistol_Jog_Fwd_Right.MF_Pistol_Jog_Fwd_Right`
- `/Game/Characters/Mannequins/Anims/Pistol/Jog/MF_Pistol_Jog_Left.MF_Pistol_Jog_Left`
- `/Game/Characters/Mannequins/Anims/Pistol/Jog/MF_Pistol_Jog_Right.MF_Pistol_Jog_Right`

## `/Game/Characters/Mannequins/Anims/Pistol/Jump`

- `/Game/Characters/Mannequins/Anims/Pistol/Jump/MM_Pistol_Jump_Fall_Loop.MM_Pistol_Jump_Fall_Loop`
- `/Game/Characters/Mannequins/Anims/Pistol/Jump/MM_Pistol_Jump_RecoveryAdditive.MM_Pistol_Jump_RecoveryAdditive`
- `/Game/Characters/Mannequins/Anims/Pistol/Jump/MM_Pistol_Jump_Start.MM_Pistol_Jump_Start`

## `/Game/Characters/Mannequins/Anims/Pistol`

- `/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS.MF_Pistol_Idle_ADS`
- `/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_DryFire.MM_Pistol_DryFire`
- `/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Equip.MM_Pistol_Equip`
- `/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Fire.MM_Pistol_Fire`
- `/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Fire_Montage.MM_Pistol_Fire_Montage`
- `/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Reload.MM_Pistol_Reload`

## `/Game/Characters/Mannequins/Anims/Pistol/Walk`

- `/Game/Characters/Mannequins/Anims/Pistol/Walk/MF_Pistol_Walk_Bwd.MF_Pistol_Walk_Bwd`
- `/Game/Characters/Mannequins/Anims/Pistol/Walk/MF_Pistol_Walk_Bwd_Left.MF_Pistol_Walk_Bwd_Left`
- `/Game/Characters/Mannequins/Anims/Pistol/Walk/MF_Pistol_Walk_Bwd_Right.MF_Pistol_Walk_Bwd_Right`
- `/Game/Characters/Mannequins/Anims/Pistol/Walk/MF_Pistol_Walk_Fwd.MF_Pistol_Walk_Fwd`
- `/Game/Characters/Mannequins/Anims/Pistol/Walk/MF_Pistol_Walk_Fwd_Left.MF_Pistol_Walk_Fwd_Left`
- `/Game/Characters/Mannequins/Anims/Pistol/Walk/MF_Pistol_Walk_Fwd_Right.MF_Pistol_Walk_Fwd_Right`
- `/Game/Characters/Mannequins/Anims/Pistol/Walk/MF_Pistol_Walk_Left.MF_Pistol_Walk_Left`
- `/Game/Characters/Mannequins/Anims/Pistol/Walk/MF_Pistol_Walk_Right.MF_Pistol_Walk_Right`

## `/Game/Characters/Mannequins/Anims/Rifle/AIM`

- `/Game/Characters/Mannequins/Anims/Rifle/AIM/AO_Rifle.AO_Rifle`
- `/Game/Characters/Mannequins/Anims/Rifle/AIM/MM_Rifle_Idle_ADS_AO_CC.MM_Rifle_Idle_ADS_AO_CC`
- `/Game/Characters/Mannequins/Anims/Rifle/AIM/MM_Rifle_Idle_ADS_AO_CD.MM_Rifle_Idle_ADS_AO_CD`
- `/Game/Characters/Mannequins/Anims/Rifle/AIM/MM_Rifle_Idle_ADS_AO_CU.MM_Rifle_Idle_ADS_AO_CU`

## `/Game/Characters/Mannequins/Anims/Rifle/HitReact`

- `/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Back_Med_01.MM_HitReact_Back_Med_01`
- `/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Hvy_01.MM_HitReact_Front_Hvy_01`
- `/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01.MM_HitReact_Front_Lgt_01`
- `/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_02.MM_HitReact_Front_Lgt_02`
- `/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_03.MM_HitReact_Front_Lgt_03`
- `/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_04.MM_HitReact_Front_Lgt_04`
- `/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Med_01.MM_HitReact_Front_Med_01`
- `/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Med_02.MM_HitReact_Front_Med_02`

## `/Game/Characters/Mannequins/Anims/Rifle/Jog`

- `/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Bwd.MF_Rifle_Jog_Bwd`
- `/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Bwd_Left.MF_Rifle_Jog_Bwd_Left`
- `/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Bwd_Right.MF_Rifle_Jog_Bwd_Right`
- `/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Fwd.MF_Rifle_Jog_Fwd`
- `/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Fwd_Left.MF_Rifle_Jog_Fwd_Left`
- `/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Fwd_Right.MF_Rifle_Jog_Fwd_Right`
- `/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Left.MF_Rifle_Jog_Left`
- `/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Right.MF_Rifle_Jog_Right`

## `/Game/Characters/Mannequins/Anims/Rifle/Jump`

- `/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Apex.MM_Rifle_Jump_Apex`
- `/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Fall_Land.MM_Rifle_Jump_Fall_Land`
- `/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Fall_Loop.MM_Rifle_Jump_Fall_Loop`
- `/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_RecoveryAdditive.MM_Rifle_Jump_RecoveryAdditive`
- `/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Start.MM_Rifle_Jump_Start`
- `/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Start_Loop.MM_Rifle_Jump_Start_Loop`

## `/Game/Characters/Mannequins/Anims/Rifle`

- `/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS.MF_Rifle_Idle_ADS`
- `/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_DryFire.MM_Rifle_DryFire`
- `/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Equip.MM_Rifle_Equip`
- `/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Fire.MM_Rifle_Fire`
- `/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload.MM_Rifle_Reload`

## `/Game/Characters/Mannequins/Anims/Rifle/Walk`

- `/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Bwd.MF_Rifle_Walk_Bwd`
- `/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Bwd_Left.MF_Rifle_Walk_Bwd_Left`
- `/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Bwd_Right.MF_Rifle_Walk_Bwd_Right`
- `/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd.MF_Rifle_Walk_Fwd`
- `/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd_Left.MF_Rifle_Walk_Fwd_Left`
- `/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd_Right.MF_Rifle_Walk_Fwd_Right`
- `/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Left.MF_Rifle_Walk_Left`
- `/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Right.MF_Rifle_Walk_Right`

## `/Game/Characters/Mannequins/Anims/Unarmed`

- `/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed`

## `/Game/Characters/Mannequins/Anims/Unarmed/Attack`

- `/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01`
- `/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02`
- `/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03.MM_Attack_03`
- `/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack.MM_ChargedAttack`

## `/Game/Characters/Mannequins/Anims/Unarmed`

- `/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run.BS_Idle_Walk_Run`

## `/Game/Characters/Mannequins/Anims/Unarmed/Jog`

- `/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Bwd.MF_Unarmed_Jog_Bwd`
- `/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Bwd_Left.MF_Unarmed_Jog_Bwd_Left`
- `/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Bwd_Right.MF_Unarmed_Jog_Bwd_Right`
- `/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd.MF_Unarmed_Jog_Fwd`
- `/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd_Left.MF_Unarmed_Jog_Fwd_Left`
- `/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd_Right.MF_Unarmed_Jog_Fwd_Right`
- `/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Left.MF_Unarmed_Jog_Left`
- `/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Right.MF_Unarmed_Jog_Right`

## `/Game/Characters/Mannequins/Anims/Unarmed/Jump`

- `/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash.MM_Dash`
- `/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Fall_Loop.MM_Fall_Loop`
- `/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Jump.MM_Jump`
- `/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Land.MM_Land`
- `/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_WallJump.MM_WallJump`

## `/Game/Characters/Mannequins/Anims/Unarmed`

- `/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle`

## `/Game/Characters/Mannequins/Anims/Unarmed/Walk`

- `/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Bwd.MF_Unarmed_Walk_Bwd`
- `/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Bwd_Left.MF_Unarmed_Walk_Bwd_Left`
- `/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Bwd_Right.MF_Unarmed_Walk_Bwd_Right`
- `/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd`
- `/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd_Left.MF_Unarmed_Walk_Fwd_Left`
- `/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd_Right.MF_Unarmed_Walk_Fwd_Right`
- `/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Left.MF_Unarmed_Walk_Left`
- `/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Right.MF_Unarmed_Walk_Right`

## `/Game/Characters/Mannequins/Materials`

- `/Game/Characters/Mannequins/Materials/M_Mannequin.M_Mannequin`

## `/Game/Characters/Mannequins/Materials/Manny`

- `/Game/Characters/Mannequins/Materials/Manny/MI_Manny_01_New.MI_Manny_01_New`
- `/Game/Characters/Mannequins/Materials/Manny/MI_Manny_02_New.MI_Manny_02_New`

## `/Game/Characters/Mannequins/Materials/Quinn`

- `/Game/Characters/Mannequins/Materials/Quinn/MI_Quinn_01.MI_Quinn_01`
- `/Game/Characters/Mannequins/Materials/Quinn/MI_Quinn_02.MI_Quinn_02`

## `/Game/Characters/Mannequins/Meshes`

- `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple`
- `/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple`
- `/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin`

## `/Game/Characters/Mannequins/Rigs`

- `/Game/Characters/Mannequins/Rigs/CR_Mannequin_Body.CR_Mannequin_Body`
- `/Game/Characters/Mannequins/Rigs/CR_Mannequin_FootIK.CR_Mannequin_FootIK`
- `/Game/Characters/Mannequins/Rigs/CR_Mannequin_Procedural.CR_Mannequin_Procedural`
- `/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin`

## `/Game/Characters/Mannequins/Textures/Manny`

- `/Game/Characters/Mannequins/Textures/Manny/T_Manny_01_BN.T_Manny_01_BN`
- `/Game/Characters/Mannequins/Textures/Manny/T_Manny_01_D.T_Manny_01_D`
- `/Game/Characters/Mannequins/Textures/Manny/T_Manny_01_MRA.T_Manny_01_MRA`
- `/Game/Characters/Mannequins/Textures/Manny/T_Manny_02_BN.T_Manny_02_BN`
- `/Game/Characters/Mannequins/Textures/Manny/T_Manny_02_D.T_Manny_02_D`
- `/Game/Characters/Mannequins/Textures/Manny/T_Manny_02_MRA.T_Manny_02_MRA`
- `/Game/Characters/Mannequins/Textures/Manny/T_Manny_02_N.T_Manny_02_N`

## `/Game/Characters/Mannequins/Textures/Quinn`

- `/Game/Characters/Mannequins/Textures/Quinn/T_Quinn_01_D.T_Quinn_01_D`
- `/Game/Characters/Mannequins/Textures/Quinn/T_Quinn_01_MRA.T_Quinn_01_MRA`
- `/Game/Characters/Mannequins/Textures/Quinn/T_Quinn_01_N.T_Quinn_01_N`
- `/Game/Characters/Mannequins/Textures/Quinn/T_Quinn_02_D.T_Quinn_02_D`
- `/Game/Characters/Mannequins/Textures/Quinn/T_Quinn_02_MRA.T_Quinn_02_MRA`
- `/Game/Characters/Mannequins/Textures/Quinn/T_Quinn_02_N.T_Quinn_02_N`

## `/Game/Characters/Mannequins/Textures/Shared`

- `/Game/Characters/Mannequins/Textures/Shared/T_UE_Logo_M.T_UE_Logo_M`
