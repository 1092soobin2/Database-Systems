SELECT AVG(cp.level)
FROM CatchedPokemon AS cp
	JOIN Trainer AS tr ON cp.owner_id = tr.id
WHERE tr.name = "RED"
;