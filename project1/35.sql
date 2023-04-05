SELECT tr.name, COUNT(*)
FROM Trainer tr
	JOIN CatchedPokemon cp ON cp.owner_id = tr.id
GROUP BY tr.id
ORDER BY tr.name
;