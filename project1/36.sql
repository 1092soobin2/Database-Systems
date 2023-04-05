SELECT pkm.name
FROM Pokemon pkm
	JOIN Evolution ev ON pkm.id = ev.after_id
WHERE type = "water"
	AND ev.after_id NOT IN (SELECT before_id
                            FROM Evolution)
;