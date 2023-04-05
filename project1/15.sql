SELECT tr.name
FROM Trainer AS tr
	JOIN CatchedPokemon AS cp ON tr.id = cp.owner_id
    JOIN Pokemon AS pkm ON cp.pid = pkm.id
WHERE tr.hometown = "Sangnok City"
	AND pkm.name LIKE "P%"
ORDER BY tr.name
;