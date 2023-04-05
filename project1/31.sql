SELECT pkm.name
FROM Trainer AS tr
	JOIN CatchedPokemon AS cp ON cp.owner_id = tr.id
    JOIN Pokemon AS pkm ON cp.pid = pkm.id
WHERE tr.hometown = "Sangnok City"
	AND cp.pid IN (SELECT cp2.pid
                   FROM Trainer AS tr2
                   	JOIN CatchedPokemon AS cp2 ON cp2.owner_id = tr2.id
                   WHERE tr2.hometown = "Blue City")
ORDER BY pkm.name
;