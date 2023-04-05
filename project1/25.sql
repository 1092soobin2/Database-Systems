SELECT pkm.name
FROM Pokemon AS pkm
	JOIN Evolution AS ev ON pkm.id = ev.after_id
WHERE ev.before_id NOT IN (SELECT after_id
                           FROM Evolution)
ORDER BY pkm.name
;