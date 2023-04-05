SELECT COUNT(*)
FROM CatchedPokemon AS cp, Pokemon AS pkm
WHERE cp.pid = pkm.id
GROUP BY pkm.type
ORDER BY pkm.type
;