SELECT SUM(cp.level)
FROM CatchedPokemon AS cp
	JOIN Trainer AS tr ON tr.id = cp.owner_id
WHERE tr.name = "Matis"
;