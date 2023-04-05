CREATE VIEW cpUp4
AS	SELECT cp.owner_id, MAX(cp.level) AS maxLevel
	FROM CatchedPokemon AS cp
	GROUP BY cp.owner_id
    HAVING COUNT(cp.id) >= 4
;

SELECT tr.name, cp.nickname
FROM Trainer AS tr, CatchedPokemon AS cp, cpUp4 AS cp4
WHERE cp.owner_id = cp4.owner_id
	AND cp.level = cp4.maxLevel
    AND tr.id = cp.owner_id
ORDER BY cp.nickname
;