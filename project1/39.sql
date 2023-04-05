SELECT pkm.name
FROM CatchedPokemon cp 
	JOIN Pokemon pkm ON pkm.id = cp.pid
    JOIN Trainer tr ON tr.id = cp.owner_id
    JOIN Gym g ON tr.id = g.leader_id
WHERE g.city = "Rainbow City"
ORDER BY pkm.name
;