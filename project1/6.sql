SELECT tr.name, AVG(cp.level)
FROM CatchedPokemon AS cp, Trainer AS tr
WHERE tr.id IN (SELECT g.leader_id FROM Gym AS g)
	AND cp.owner_id = tr.id
GROUP BY cp.owner_id
ORDER BY tr.name
;