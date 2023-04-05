SELECT pkm.name
FROM Pokemon AS pkm, Evolution AS ev
WHERE ev.before_id > ev.after_id
	AND pkm.id = ev.before_id
ORDER BY pkm.name
;