#pragma once
#include <string>
// Synthetic fixture matching the companion server's embedded schema, no live URLs.
inline const std::string MetadataFixture=R"({
 "schema_version":1,"id":123,"url":"https://www.igdb.com/games/sample-game",
 "summary":"Explore an abandoned station and uncover its secrets.",
 "storyline":"A pilot searches for a lost expedition.\nThe journey changes everything.",
 "publishers":["Example Publisher"],"developers":["Example Studio"],
 "genres":["Adventure","Platform","Adventure"],"themes":["Science fiction","Action"],
 "ps4_release_dates":[{"date":"2020-02-02","region":"worldwide"}],
 "ratings":{"users":{"score":84,"count":123},"critics":{"score":80,"count":12}},
 "game_modes":["Single player","Co-operative"],
 "multiplayer":{"offline":{"supported":true,"max_players":2,"co_op":true,"split_screen":true},"online":{"supported":false,"split_screen":false}},
 "languages":{"interface":["English","French"],"subtitles":["English"],"voice":["English"]},
 "age_ratings":[{"organization":"ESRB","rating":"T","content_descriptions":["Fantasy violence"],"synopsis":null}],
 "media":{"screenshots":[{"url":"https://images.example.org/scene.jpg","width":1920,"height":1080},{"url":"//images.example.org/scene2.jpg"}]},
 "videos":[{"name":"Trailer","video_id":"abcdefghijk"},{"name":"Gameplay","url":"https://video.example.org/gameplay"}],
 "relationships":{"dlcs":[{"id":456,"name":"Sample expansion"}],"parent_game":null},
 "websites":[{"type":"official","url":"https://example.org/game","trusted":true}],
 "future_metadata":{"nested":{"retained":true}}
})";
