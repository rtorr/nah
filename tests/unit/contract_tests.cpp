#include <doctest/doctest.h>
#include <nah/contract.hpp>
#include <nah/types.hpp>

using namespace nah;

// Unit tests for contract serialization and data structures
// Full compose_contract testing requires actual files and is done in integration tests

TEST_CASE("serialize_contract_json produces deterministic output") {
    ContractEnvelope envelope;
    envelope.contract.app.id = "com.example.app";
    envelope.contract.app.version = "1.0.0";
    envelope.contract.environment["Z_VAR"] = "z";
    envelope.contract.environment["A_VAR"] = "a";
    envelope.contract.environment["M_VAR"] = "m";
    
    std::string json1 = serialize_contract_json(envelope, false, std::nullopt);
    std::string json2 = serialize_contract_json(envelope, false, std::nullopt);
    
    // Same input should produce identical output
    CHECK(json1 == json2);
    
    // Keys should be in lexicographic order
    size_t pos_a = json1.find("A_VAR");
    size_t pos_m = json1.find("M_VAR");
    size_t pos_z = json1.find("Z_VAR");
    
    CHECK(pos_a < pos_m);
    CHECK(pos_m < pos_z);
}

TEST_CASE("serialize_contract_json includes schema") {
    ContractEnvelope envelope;
    envelope.contract.app.id = "test.app";
    envelope.contract.app.version = "1.0.0";
    
    std::string json = serialize_contract_json(envelope, false, std::nullopt);
    
    CHECK(json.find("nah.launch.contract.v1") != std::string::npos);
}

TEST_CASE("serialize_contract_json includes app fields") {
    ContractEnvelope envelope;
    envelope.contract.app.id = "com.example.myapp";
    envelope.contract.app.version = "2.5.0";
    envelope.contract.app.root = "/nah/apps/myapp";
    envelope.contract.app.entrypoint = "/nah/apps/myapp/bin/run";
    
    std::string json = serialize_contract_json(envelope, false, std::nullopt);
    
    CHECK(json.find("com.example.myapp") != std::string::npos);
    CHECK(json.find("2.5.0") != std::string::npos);
    CHECK(json.find("/nah/apps/myapp") != std::string::npos);
}

TEST_CASE("serialize_contract_json includes nak fields when present") {
    ContractEnvelope envelope;
    envelope.contract.app.id = "test.app";
    envelope.contract.app.version = "1.0.0";
    envelope.contract.nak.id = "com.example.nak";
    envelope.contract.nak.version = "3.0.0";
    envelope.contract.nak.root = "/nah/naks/nak/3.0.0";
    
    std::string json = serialize_contract_json(envelope, false, std::nullopt);
    
    CHECK(json.find("com.example.nak") != std::string::npos);
    CHECK(json.find("3.0.0") != std::string::npos);
}

TEST_CASE("serialize_contract_json includes execution fields") {
    ContractEnvelope envelope;
    envelope.contract.app.id = "test.app";
    envelope.contract.app.version = "1.0.0";
    envelope.contract.execution.binary = "/path/to/binary";
    envelope.contract.execution.cwd = "/working/dir";
    envelope.contract.execution.library_paths = {"/lib1", "/lib2"};
    
    std::string json = serialize_contract_json(envelope, false, std::nullopt);
    
    CHECK(json.find("/path/to/binary") != std::string::npos);
    CHECK(json.find("/working/dir") != std::string::npos);
}

TEST_CASE("serialize_contract_json includes environment") {
    ContractEnvelope envelope;
    envelope.contract.app.id = "test.app";
    envelope.contract.app.version = "1.0.0";
    envelope.contract.environment["MY_VAR"] = "my_value";
    envelope.contract.environment["OTHER_VAR"] = "other_value";
    
    std::string json = serialize_contract_json(envelope, false, std::nullopt);
    
    CHECK(json.find("MY_VAR") != std::string::npos);
    CHECK(json.find("my_value") != std::string::npos);
}

TEST_CASE("serialize_contract_json includes warnings") {
    ContractEnvelope envelope;
    envelope.contract.app.id = "test.app";
    envelope.contract.app.version = "1.0.0";
    
    WarningObject w;
    w.key = "test_warning";
    w.action = "warn";
    w.fields["detail"] = "some detail";
    envelope.warnings.push_back(w);
    
    std::string json = serialize_contract_json(envelope, false, std::nullopt);
    
    CHECK(json.find("test_warning") != std::string::npos);
    CHECK(json.find("warn") != std::string::npos);
}

TEST_CASE("serialize_contract_json handles critical error") {
    ContractEnvelope envelope;
    envelope.contract.app.id = "test.app";
    envelope.contract.app.version = "1.0.0";
    
    std::string json = serialize_contract_json(envelope, false, CriticalError::MANIFEST_MISSING);
    
    CHECK(json.find("critical_error") != std::string::npos);
    CHECK(json.find("MANIFEST_MISSING") != std::string::npos);
}

TEST_CASE("serialize_contract_json includes trace when requested") {
    ContractEnvelope envelope;
    envelope.contract.app.id = "com.example.app";
    envelope.contract.app.version = "1.0.0";
    
    // Initialize trace map
    std::unordered_map<std::string, std::unordered_map<std::string, TraceEntry>> trace_map;
    TraceEntry t1;
    t1.value = "step1_value";
    t1.source_kind = "profile";
    t1.source_path = "/path/to/profile";
    t1.precedence_rank = 1;
    trace_map["environment"]["VAR1"] = t1;
    envelope.trace = trace_map;
    
    std::string without_trace = serialize_contract_json(envelope, false, std::nullopt);
    std::string with_trace = serialize_contract_json(envelope, true, std::nullopt);
    
    CHECK(without_trace.find("trace") == std::string::npos);
    CHECK(with_trace.find("trace") != std::string::npos);
}

TEST_CASE("serialize_contract_json trace contains all entry fields") {
    ContractEnvelope envelope;
    envelope.contract.app.id = "com.example.app";
    envelope.contract.app.version = "1.0.0";
    
    std::unordered_map<std::string, std::unordered_map<std::string, TraceEntry>> trace_map;
    TraceEntry entry;
    entry.value = "test_value";
    entry.source_kind = "install_record";
    entry.source_path = "/nah/registry/installs/app.json";
    entry.precedence_rank = 4;
    trace_map["environment"]["MY_VAR"] = entry;
    envelope.trace = trace_map;
    
    std::string json = serialize_contract_json(envelope, true, std::nullopt);
    
    // Verify trace structure contains all fields
    CHECK(json.find("\"trace\"") != std::string::npos);
    CHECK(json.find("\"environment\"") != std::string::npos);
    CHECK(json.find("\"MY_VAR\"") != std::string::npos);
    CHECK(json.find("\"value\"") != std::string::npos);
    CHECK(json.find("\"test_value\"") != std::string::npos);
    CHECK(json.find("\"source_kind\"") != std::string::npos);
    CHECK(json.find("\"install_record\"") != std::string::npos);
    CHECK(json.find("\"source_path\"") != std::string::npos);
    CHECK(json.find("\"precedence_rank\"") != std::string::npos);
}

TEST_CASE("serialize_contract_json trace is deterministically ordered") {
    ContractEnvelope envelope;
    envelope.contract.app.id = "test";
    
    std::unordered_map<std::string, std::unordered_map<std::string, TraceEntry>> trace_map;
    TraceEntry e1, e2, e3;
    e1.value = "a"; e1.source_kind = "profile"; e1.precedence_rank = 1;
    e2.value = "b"; e2.source_kind = "manifest"; e2.precedence_rank = 3;
    e3.value = "c"; e3.source_kind = "standard"; e3.precedence_rank = 5;
    
    // Add in non-sorted order
    trace_map["environment"]["ZEBRA"] = e1;
    trace_map["environment"]["ALPHA"] = e2;
    trace_map["app"]["id"] = e3;
    envelope.trace = trace_map;
    
    std::string json1 = serialize_contract_json(envelope, true, std::nullopt);
    std::string json2 = serialize_contract_json(envelope, true, std::nullopt);
    
    // Must be identical (deterministic)
    CHECK(json1 == json2);
    
    // Verify alphabetical ordering: "app" before "environment", "ALPHA" before "ZEBRA"
    auto app_pos = json1.find("\"app\"");
    auto env_pos = json1.find("\"environment\"");
    CHECK(app_pos < env_pos);
    
    auto alpha_pos = json1.find("\"ALPHA\"");
    auto zebra_pos = json1.find("\"ZEBRA\"");
    CHECK(alpha_pos < zebra_pos);
}

TEST_CASE("parse_overrides_file parses JSON") {
    std::string json = R"({
        "environment": {
            "MY_VAR": "value1",
            "OTHER": "value2"
        },
        "warnings": {
            "some_warning": "ignore"
        }
    })";
    
    auto result = parse_overrides_file(json, "test.json");
    
    CHECK(result.ok);
    CHECK(result.overrides.environment.size() == 2);
    CHECK(result.overrides.environment["MY_VAR"] == "value1");
    CHECK(result.overrides.warnings.size() == 1);
}

TEST_CASE("parse_overrides_file rejects invalid JSON") {
    std::string json = "not valid json";
    
    auto result = parse_overrides_file(json, "test.json");
    
    CHECK_FALSE(result.ok);
}

TEST_CASE("get_library_path_env_key returns platform-appropriate key") {
    std::string key = get_library_path_env_key();
    
#if defined(__APPLE__)
    CHECK(key == "DYLD_LIBRARY_PATH");
#elif defined(_WIN32)
    CHECK(key == "PATH");
#else
    CHECK(key == "LD_LIBRARY_PATH");
#endif
}

TEST_CASE("get_path_separator returns platform-appropriate separator") {
    char sep = get_path_separator();
    
#ifdef _WIN32
    CHECK(sep == ';');
#else
    CHECK(sep == ':');
#endif
}
