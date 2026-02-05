Scriptname TTON_DeclineEffect extends activemagiceffect

Spell Property currentSpell Auto

Event OnUpdate()
    Actor akTarget = GetTargetActor()
    akTarget.RemoveSpell(currentSpell)
EndEvent

Event OnEffectStart(Actor akTarget, Actor akCaster)
	; start timer
	RegisterForSingleUpdate(TTON_JData.GetMcmDenyCooldown())
EndEvent
