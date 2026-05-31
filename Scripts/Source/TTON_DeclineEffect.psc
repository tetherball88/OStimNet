Scriptname TTON_DeclineEffect extends activemagiceffect

Spell Property currentSpell Auto

Event OnUpdate()
    Actor akTarget = GetTargetActor()
    akTarget.RemoveSpell(currentSpell)
EndEvent
