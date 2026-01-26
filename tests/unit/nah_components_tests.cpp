/**
 * Unit tests for component functionality
 */

#include <nah/nah_core.h>
#include <nah/nah_json.h>
#include <doctest/doctest.h>

using namespace nah::core;

TEST_CASE("Component URI parsing") {
    SUBCASE("Valid URI with path") {
        auto uri = parse_component_uri("com.example.suite://editor/open");
        CHECK(uri.valid);
        CHECK(uri.app_id == "com.example.suite");
        CHECK(uri.component_path == "editor/open");
        CHECK(uri.query.empty());
        CHECK(uri.fragment.empty());
    }
    
    SUBCASE("Valid URI with query") {
        auto uri = parse_component_uri("com.suite://editor?file=doc.txt");
        CHECK(uri.valid);
        CHECK(uri.app_id == "com.suite");
        CHECK(uri.component_path == "editor");
        CHECK(uri.query == "file=doc.txt");
    }
    
    SUBCASE("Valid URI with fragment") {
        auto uri = parse_component_uri("com.suite://viewer#section-3");
        CHECK(uri.valid);
        CHECK(uri.fragment == "section-3");
    }
    
    SUBCASE("Valid URI with query and fragment") {
        auto uri = parse_component_uri("com.suite://editor?file=doc.txt#line42");
        CHECK(uri.valid);
        CHECK(uri.query == "file=doc.txt");
        CHECK(uri.fragment == "line42");
    }
    
    SUBCASE("Invalid URI - no scheme separator") {
        auto uri = parse_component_uri("com.suite/editor");
        CHECK_FALSE(uri.valid);
    }
    
    SUBCASE("Invalid URI - empty app ID") {
        auto uri = parse_component_uri("://editor");
        CHECK_FALSE(uri.valid);
    }
    
    SUBCASE("Empty URI") {
        auto uri = parse_component_uri("");
        CHECK_FALSE(uri.valid);
    }
}

TEST_CASE("Component manifest parsing") {
    SUBCASE("Parse component from JSON") {
        std::string json_str = R"({
            "id": "editor",
            "name": "Document Editor",
            "entrypoint": "bin/editor",
            "uri_pattern": "com.suite://editor/*",
            "loader": "default",
            "standalone": true,
            "hidden": false
        })";
        
        auto j = nlohmann::json::parse(json_str);
        auto comp = nah::json::parse_component(j);
        
        CHECK(comp.id == "editor");
        CHECK(comp.name == "Document Editor");
        CHECK(comp.entrypoint == "bin/editor");
        CHECK(comp.uri_pattern == "com.suite://editor/*");
        CHECK(comp.loader == "default");
        CHECK(comp.standalone);
        CHECK_FALSE(comp.hidden);
    }
    
    SUBCASE("Parse component with minimal fields") {
        std::string json_str = R"({
            "id": "viewer",
            "entrypoint": "bin/viewer",
            "uri_pattern": "com.suite://viewer/*"
        })";
        
        auto j = nlohmann::json::parse(json_str);
        auto comp = nah::json::parse_component(j);
        
        CHECK(comp.id == "viewer");
        CHECK(comp.entrypoint == "bin/viewer");
        CHECK(comp.uri_pattern == "com.suite://viewer/*");
        CHECK(comp.standalone);  // default is true
        CHECK_FALSE(comp.hidden);  // default is false
    }
    
    SUBCASE("Parse component with hidden flag") {
        std::string json_str = R"({
            "id": "internal",
            "entrypoint": "bin/internal",
            "uri_pattern": "com.suite://internal/*",
            "standalone": false,
            "hidden": true
        })";
        
        auto j = nlohmann::json::parse(json_str);
        auto comp = nah::json::parse_component(j);
        
        CHECK_FALSE(comp.standalone);
        CHECK(comp.hidden);
    }
}

TEST_CASE("App manifest with components") {
    SUBCASE("Parse app with components") {
        std::string json_str = R"({
            "app": {
                "identity": {
                    "id": "com.example.suite",
                    "version": "1.0.0"
                },
                "execution": {
                    "entrypoint": "bin/launcher"
                },
                "components": {
                    "provides": [
                        {
                            "id": "editor",
                            "entrypoint": "bin/editor",
                            "uri_pattern": "com.example.suite://editor/*"
                        },
                        {
                            "id": "viewer",
                            "entrypoint": "bin/viewer",
                            "uri_pattern": "com.example.suite://viewer/*"
                        }
                    ]
                }
            }
        })";
        
        auto result = nah::json::parse_app_declaration(json_str);
        CHECK(result.ok);
        CHECK(result.value.components.size() == 2);
        CHECK(result.value.components[0].id == "editor");
        CHECK(result.value.components[1].id == "viewer");
    }
    
    SUBCASE("Parse app without components") {
        std::string json_str = R"({
            "app": {
                "identity": {
                    "id": "com.example.app",
                    "version": "1.0.0"
                },
                "execution": {
                    "entrypoint": "bin/app"
                }
            }
        })";
        
        auto result = nah::json::parse_app_declaration(json_str);
        CHECK(result.ok);
        CHECK(result.value.components.empty());
    }
}
