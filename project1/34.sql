SELECT pkm2.name
FROM Pokemon AS pkm2
	JOIN Evolution AS ev2 ON pkm2.id = ev2.after_id
WHERE ev2.before_id IN (SELECT ev.after_id
                       FROM Pokemon AS pkm, Evolution AS ev
                       WHERE pkm.name = "Charmander"
                       AND pkm.id = ev.before_id) 
;