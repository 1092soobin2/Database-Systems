CREATE VIEW Id_LevelSum 
AS
SELECT tr.id, tr.name, SUM(cp.level) AS levelSum
FROM Trainer AS tr
	JOIN CatchedPokemon AS cp ON cp.owner_id = tr.id
GROUP BY tr.id
;

SELECT name, levelSum
FROM Id_LevelSum
WHERE levelSum >= ALL (SELECT levelSum From Id_LevelSum)
;