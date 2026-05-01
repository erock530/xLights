#include "ai/OpenAIAPI.h"
#include "ai/OpenAIImageGenerator.h"
#include "ai/ServiceManager.h"

#include "utils/CurlManager.h"
#include "utils/UtilFunctions.h"
#include "utils/string_utils.h"

#include <nlohmann/json.hpp>

#include <log.h>

#include <curl/curl.h>

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <vector>

constexpr const char* completion_url = "/chat/completions";

constexpr const char* transcriptions_url = "/audio/transcriptions";

std::pair<std::string, bool> OpenAIAPI::CallLLM(const std::string& prompt) const {
    std::string bearerToken = token;

    if (bearerToken.empty()) {
        return { GetLLMName() + ": Bearer Token is empty", false };
    }

    // remove all \t, \r and \n as chatGPT does not like it
    std::string p = prompt;
    Replace(p, std::string("\t"), std::string(" "));
    Replace(p, std::string("\r"), std::string(""));
    Replace(p, std::string("\n"), std::string("\\n"));

    std::string const request = "{ \"model\": \"" + model + "\", \"messages\": [ { \"role\": \"user\",\"content\": \"" + JSONSafe(p) + "\" } ] }";

    std::vector<std::pair<std::string, std::string>> customHeaders = {
        { "Authorization", "Bearer " + bearerToken }
    };

    spdlog::debug("{}: {}", GetLLMName(), request);

    int responseCode = 0;
    std::string response = CurlManager::HTTPSPost(base_url + completion_url, request, "", "", "JSON", 60, customHeaders, &responseCode);

    spdlog::debug("{} Response {}: {}", GetLLMName(), responseCode, response);

    if (responseCode != 200) {
        return { response, false };
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(response);
    } catch (const std::exception&) {
        spdlog::error("{}: Invalid JSON response: {}", GetLLMName(), response);
        return { GetLLMName() +  ": Invalid JSON response", false };
    }

    auto choices = root["choices"];
    if (choices.is_null() || choices.size() == 0) {
        spdlog::error("{}: No choices in response", GetLLMName());
        return { GetLLMName() + ": No choices in response", false };
    }

    auto choice = choices[0];
    auto text = choice["message"]["content"];
    if (text.is_null()) {
        spdlog::error("{}: No text in response", GetLLMName());
        return { GetLLMName() + ": No text in response", false };
    }

    response = text.get<std::string>();
    spdlog::debug("{}: {}", GetLLMName(), response);

    return { response, true };
}

aiBase::AIColorPalette OpenAIAPI::GenerateColorPalette(const std::string& prompt) const {
    aiBase::AIColorPalette ret;
    if (token.empty()) {
        ret.error = "You must set a " + GetLLMName() + " Bearer Token in the Preferences on the Services Panel";
        return ret;
    }

    std::string fullprompt = "xlights color palettes are 8 unique colors. Can you create a color palette that would represent the moods and imagery " + prompt + ". Avoid dark, near black colors.";

    std::string schema = R"(
    "response_format" : {
        "type": "json_schema",
        "json_schema": {
            "name": "color_palette",
            "schema": {
                "type": "object",
                "properties":  {
                    "description": {
                        "type": "string"
                    },
                    "colors": {
                        "type": "array",
                        "items": {
                            "type": "object",
                            "properties": {
                                "hex_code": {
                                    "type": "string"
                                },
                                "name": {
                                    "type": "string"
                                },
                                "usage_notes": {
                                    "type": "string"
                                }
                            },
                            "required": [
                                "hex_code", "name", "usage_notes"
                            ]
                        }
                    }
                },
                "required": [
                    "description", "colors"
                ]
            }
        }
    }
    )";

    std::string const request = "{ \"model\": \"" + model + "\", \"messages\": [ { \"role\": \"user\",\"content\": \"" + JSONSafe(fullprompt) + "\" } ]," + schema + "}";

    std::vector<std::pair<std::string, std::string>> customHeaders = {
        { "Authorization", "Bearer " + token }
    };

    spdlog::debug("{}: {}", GetLLMName(), request);

    int responseCode = 0;
    std::string const response = CurlManager::HTTPSPost(base_url + completion_url, request, "", "", "JSON", 60, customHeaders, &responseCode);

    spdlog::debug("{} Response {}: {}", GetLLMName(), responseCode, response);
    if (responseCode != 200) {
        ret.error = response;
    }
    spdlog::debug("{} Response: {}", GetLLMName(), response);
    try {
        nlohmann::json root = nlohmann::json::parse(response);

        if (root.contains("choices") && root["choices"].is_array() && !root["choices"].empty() && root["choices"][0].contains("message")) {
            auto const color_responce = root["choices"][0]["message"]["content"].get<std::string>();

            spdlog::debug("{} Content {}", GetLLMName(), color_responce);
            try {
                nlohmann::json const color_root = nlohmann::json::parse(color_responce);
                if (color_root.contains("colors") && color_root["colors"].is_array()) {
                    aiBase::AIColorPalette out;
                    out.description = prompt;
                    if (color_root.contains("description")) {
                        out.description = color_root["description"].get<std::string>();
                    }
                    for (size_t x = 0; x < color_root["colors"].size(); x++) {
                        auto& color = color_root["colors"][x];
                        out.colors.push_back(aiBase::AIColor());
                        out.colors.back().hexValue = color["hex_code"].get<std::string>();
                        out.colors.back().description = color["usage_notes"].get<std::string>();
                        out.colors.back().name = color["name"].get<std::string>();
                    }
                    return out;
                }
            } catch (const std::exception& ex) {
                spdlog::error("{}", ex.what());
            }
            spdlog::error("Response does not contain 'colors' array or is not in expected format.");
            ret.error = "Response does not contain 'colors' array or is not in expected format.";
        } else {
            spdlog::error("Invalid response from {} API.", GetLLMName());
            ret.error = "Invalid response from " + GetLLMName() + " API.";
        }
    } catch (const std::exception& e) {
        spdlog::error("{}", e.what());
    }

    return ret;
}

aiBase::AIMusicEffectPlan OpenAIAPI::GenerateMusicEffectPlan(
    const AIMusicAnalysis& analysis,
    const std::vector<MappingModelInfo>& targets,
    const AIMusicGenerationOptions& options) const {
    AIMusicEffectPlan plan;
    if (token.empty()) {
        plan.error = "You must set a " + GetLLMName() + " Bearer Token in the Preferences on the Services Panel";
        return plan;
    }
    if (targets.empty()) {
        plan.error = "No effect targets were provided.";
        return plan;
    }

    nlohmann::json analysisJson;
    analysisJson["startMS"] = analysis.startMS;
    analysisJson["endMS"] = analysis.endMS;
    analysisJson["bpm"] = analysis.bpm;
    analysisJson["beatMS"] = analysis.beatMS;
    analysisJson["downbeatMS"] = analysis.downbeatMS;
    analysisJson["sectionMS"] = analysis.sectionMS;
    analysisJson["energy"] = analysis.energy;
    analysisJson["onsetDensity"] = analysis.onsetDensity;

    nlohmann::json targetJson = nlohmann::json::array();
    for (const auto& t : targets) {
        targetJson.push_back({
            { "name", t.name },
            { "type", t.type },
            { "modelClass", t.modelClass },
            { "nameHints", t.aliases },
            { "nodeCount", t.nodeCount },
            { "width", t.width },
            { "height", t.height }
        });
    }

    nlohmann::json optionsJson;
    optionsJson["style"] = options.style;
    optionsJson["intensity"] = options.intensity;
    optionsJson["density"] = options.density;
    optionsJson["allowedEffects"] = options.allowedEffects;
    optionsJson["overwritePolicy"] = options.overwritePolicy;
    optionsJson["startMS"] = options.startMS;
    optionsJson["endMS"] = options.endMS;
    optionsJson["songName"] = options.songName;

    auto trimJsonEnvelope = [](std::string s) {
        if (StartsWith(s, "```")) {
            auto firstNewline = s.find('\n');
            if (firstNewline != std::string::npos) {
                s = s.substr(firstNewline + 1);
            }
            auto fence = s.rfind("```");
            if (fence != std::string::npos) {
                s = s.substr(0, fence);
            }
        }
        return Trim(s);
    };

    std::string aiMusicAnalysisJson = "{}";
    std::string aiClipTranscriptJson = "{}";
    if (!options.mediaPath.empty()) {
        auto clipTranscript = GenerateLyricTrack(options.mediaPath);
        if (!clipTranscript.error.empty()) {
            plan.warnings.push_back("AI clip-transcription pass failed: " + clipTranscript.error);
        } else if (!clipTranscript.lyrics.empty()) {
            nlohmann::json transcriptJson;
            transcriptJson["source"] = "uploaded_media_clip";
            transcriptJson["songName"] = options.songName;
            transcriptJson["wordCount"] = clipTranscript.lyrics.size();
            transcriptJson["words"] = nlohmann::json::array();
            const size_t maxWords = std::min<size_t>(clipTranscript.lyrics.size(), 240);
            for (size_t i = 0; i < maxWords; ++i) {
                const auto& word = clipTranscript.lyrics[i];
                transcriptJson["words"].push_back({
                    {"word", word.word},
                    {"startMS", word.startMS},
                    {"endMS", word.endMS}
                });
            }
            if (clipTranscript.lyrics.size() > maxWords) {
                transcriptJson["truncated"] = true;
            }
            aiClipTranscriptJson = transcriptJson.dump();
        } else {
            plan.warnings.push_back("AI clip-transcription pass returned no words from the uploaded media clip.");
        }
    } else {
        plan.warnings.push_back("No media clip path provided to AI planner; using feature-derived analysis only.");
    }

    std::string analysisPrompt =
        "You are analyzing music structure for xLights effect planning. Return JSON only.\n"
        "JSON schema:\n"
        "{"
        "\"overallMood\":\"short description\","
        "\"overallEnergy\":\"Low|Medium|High\","
        "\"sections\":[{\"startMS\":0,\"endMS\":1000,\"energy\":\"Low|Medium|High\",\"intent\":\"short phrase\"}],"
        "\"guidance\":[\"short directive\"]"
        "}\n"
        "Rules:\n"
        "- Use the supplied Analysis JSON cues and media clip transcript/timestamps when available.\n"
        "- Use songName as additional context for style/mood inference when available.\n"
        "- Infer a practical progression: intro/build/peaks/breaks/outro where supported by data.\n"
        "- Keep sections inside requested startMS/endMS.\n"
        "- 4-12 sections max.\n"
        "- No markdown fences.\n"
        "Media Clip Transcript JSON:\n" + aiClipTranscriptJson +
        "\n"
        "Analysis JSON:\n" + analysisJson.dump() +
        "\nOptions JSON:\n" + optionsJson.dump();

    auto [analysisResponse, analysisOk] = CallLLM(analysisPrompt);
    if (analysisOk) {
        aiMusicAnalysisJson = trimJsonEnvelope(analysisResponse);
        if (aiMusicAnalysisJson.empty()) {
            aiMusicAnalysisJson = "{}";
        } else if (aiMusicAnalysisJson.size() > 12000) {
            aiMusicAnalysisJson = aiMusicAnalysisJson.substr(0, 12000);
        }
    } else {
        plan.warnings.push_back("AI music-analysis pass failed; generating effects from raw analysis cues.");
    }

    const size_t minimumCoverageBlocks = std::max<size_t>(24, targets.size() * 3);
    std::string initialPlanResponsePreview;
    std::string retryPlanResponsePreview;
    std::string rescuePlanResponsePreview;
    auto parsePlanResponse = [&](const std::string& responseText, AIMusicEffectPlan& outPlan) -> bool {
        try {
            auto getString = [](const nlohmann::json& obj, const std::initializer_list<const char*> keys) -> std::string {
                for (const auto* key : keys) {
                    if (!obj.contains(key)) {
                        continue;
                    }
                    const auto& v = obj[key];
                    if (v.is_string()) {
                        return v.get<std::string>();
                    }
                    if (v.is_number_integer()) {
                        return std::to_string(v.get<int>());
                    }
                    if (v.is_number_float()) {
                        return std::to_string(v.get<double>());
                    }
                }
                return {};
            };
            auto getInt = [](const nlohmann::json& obj, const std::initializer_list<const char*> keys, int fallback) -> int {
                for (const auto* key : keys) {
                    if (!obj.contains(key)) {
                        continue;
                    }
                    const auto& v = obj[key];
                    if (v.is_number_integer()) {
                        return v.get<int>();
                    }
                    if (v.is_number_float()) {
                        return static_cast<int>(v.get<double>());
                    }
                    if (v.is_string()) {
                        const std::string s = Trim(v.get<std::string>());
                        if (!s.empty()) {
                            char* endPtr = nullptr;
                            const long parsed = std::strtol(s.c_str(), &endPtr, 10);
                            if (endPtr != s.c_str()) {
                                return static_cast<int>(parsed);
                            }
                        }
                    }
                }
                return fallback;
            };
            auto normalizePreview = [](const std::string& raw) {
                std::string normalized = raw;
                Replace(normalized, std::string("\n"), std::string(" "));
                Replace(normalized, std::string("\r"), std::string(" "));
                Replace(normalized, std::string("\t"), std::string(" "));
                if (normalized.size() > 360) {
                    normalized = normalized.substr(0, 360) + "...";
                }
                return normalized;
            };
            auto hasTimingFields = [&](const nlohmann::json& obj) {
                const int start = getInt(obj, {"startMS", "startMs", "start", "start_time_ms"}, -1);
                const int end = getInt(obj, {"endMS", "endMs", "end", "end_time_ms"}, -1);
                const int duration = getInt(obj, {"durationMS", "durationMs", "duration", "lengthMS"}, -1);
                return (start >= 0 && end > start) || (start >= 0 && duration > 0);
            };

            nlohmann::json root = nlohmann::json::parse(trimJsonEnvelope(responseText));
            const nlohmann::json* blockContainer = nullptr;
            if (root.is_array()) {
                blockContainer = &root;
            } else if (root.contains("blocks") && root["blocks"].is_array()) {
                blockContainer = &root["blocks"];
            } else if (root.contains("plan") && root["plan"].is_object() && root["plan"].contains("blocks") && root["plan"]["blocks"].is_array()) {
                blockContainer = &root["plan"]["blocks"];
            } else if (root.contains("timeline") && root["timeline"].is_array()) {
                blockContainer = &root["timeline"];
            } else if (root.contains("effects") && root["effects"].is_array()) {
                blockContainer = &root["effects"];
            }
            if (blockContainer == nullptr) {
                outPlan.error = "AI response did not include a recognized block container.";
                outPlan.warnings.push_back("AI plan parse preview: " + normalizePreview(responseText));
                return false;
            }
            if (root.contains("warnings") && root["warnings"].is_array()) {
                for (const auto& w : root["warnings"]) {
                    if (w.is_string()) {
                        outPlan.warnings.push_back(w.get<std::string>());
                    }
                }
            }

            int candidateObjects = 0;
            std::function<void(const nlohmann::json&, const std::string&, const std::string&)> parseNode;
            parseNode = [&](const nlohmann::json& node, const std::string& inheritedTarget, const std::string& inheritedEffect) {
                if (node.is_array()) {
                    for (const auto& child : node) {
                        parseNode(child, inheritedTarget, inheritedEffect);
                    }
                    return;
                }
                if (!node.is_object()) {
                    return;
                }
                ++candidateObjects;

                std::string targetName = getString(node, {"targetName", "target", "targetModel", "model", "modelName"});
                if (targetName.empty()) {
                    targetName = inheritedTarget;
                }
                std::string effectName = getString(node, {"effectName", "effect", "effectType", "name"});
                if (effectName.empty()) {
                    effectName = inheritedEffect;
                }

                bool emitted = false;
                if (!targetName.empty() && !effectName.empty() && hasTimingFields(node)) {
                    emitted = true;
                AIEffectBlock block;
                    block.targetName = targetName;
                    block.layerHint = getInt(node, {"layerHint", "layer", "layerIndex"}, -1);
                    block.effectName = effectName;
                    block.startMS = getInt(node, {"startMS", "startMs", "start", "start_time_ms"}, 0);
                    block.endMS = getInt(node, {"endMS", "endMs", "end", "end_time_ms"}, 0);
                    if (block.endMS <= block.startMS) {
                        int duration = getInt(node, {"durationMS", "durationMs", "duration", "lengthMS"}, 0);
                        if (duration > 0) {
                            block.endMS = block.startMS + duration;
                        }
                    }
                    block.palette = getString(node, {"palette", "colorPalette"});
                    if (node.contains("confidence")) {
                        if (node["confidence"].is_number()) {
                            block.confidence = node["confidence"].get<float>();
                        } else if (node["confidence"].is_string()) {
                            block.confidence = static_cast<float>(std::strtod(node["confidence"].get<std::string>().c_str(), nullptr));
                        }
                    }
                    block.reason = getString(node, {"reason", "why", "rationale"});
                    block.priority = getInt(node, {"priority", "rank", "order"}, 0);

                    if (node.contains("settings") && node["settings"].is_object()) {
                        for (auto it = node["settings"].begin(); it != node["settings"].end(); ++it) {
                            if (it.value().is_string()) {
                                block.settings[it.key()] = it.value().get<std::string>();
                            } else {
                                block.settings[it.key()] = it.value().dump();
                            }
                        }
                    }
                    outPlan.blocks.push_back(std::move(block));
                }

                static const std::vector<std::string> nestedKeys = {"blocks", "segments", "events", "items", "children"};
                for (const auto& key : nestedKeys) {
                    if (node.contains(key) && node[key].is_array()) {
                        parseNode(node[key], targetName, effectName);
                    }
                }

                if (!emitted && node.contains("windows") && node["windows"].is_array()) {
                    parseNode(node["windows"], targetName, effectName);
                }
            };

            parseNode(*blockContainer, "", "");
            if (outPlan.blocks.empty() && candidateObjects > 0) {
                outPlan.warnings.push_back(
                    "AI plan parse found " + std::to_string(candidateObjects) +
                    " object node(s) but none mapped to valid effect blocks. Preview: " +
                    normalizePreview(responseText));
            } else if (outPlan.blocks.empty()) {
                outPlan.warnings.push_back("AI plan parse preview: " + normalizePreview(responseText));
            }
            return true;
        } catch (const std::exception& ex) {
            outPlan.error = std::string("Failed parsing AI music effects plan: ") + ex.what();
            return false;
        }
    };

    auto buildPlanPrompt = [&](const std::string& extraRules) {
        return
        "You are generating xLights effect plans. Return JSON only.\n"
        "JSON schema:\n"
        "{"
        "\"blocks\": ["
        "{"
        "\"targetName\": \"string\","
        "\"layerHint\": 0,"
        "\"effectName\": \"string from allowedEffects\","
        "\"startMS\": 0,"
        "\"endMS\": 1000,"
        "\"settings\": {\"key\": \"value\"},"
        "\"palette\": \"optional palette string\","
        "\"confidence\": 0.0,"
        "\"reason\": \"short reason\","
        "\"priority\": 0"
        "}"
        "],"
        "\"warnings\": [\"string\"]"
        "}\n"
        "Rules:\n"
        "- Use only provided targets.\n"
        "- Use songName for additional context (title/theme) when available.\n"
        "- Use target names and nameHints to infer what each prop likely represents (tree, star, roofline, matrix, singing face, etc).\n"
        "- Use each target's type/modelClass/nodeCount/width/height to vary choices by model shape.\n"
        "- Avoid cloning identical block timings and effect names across all targets; stagger or alternate where sensible.\n"
        "- Prefer bars/linear style looks for arches/lines, matrix-style looks for matrixes, and broad washes for groups.\n"
        "- Align transitions to beat/downbeat/section cues from Analysis JSON so effects follow the song structure.\n"
        "- Keep all blocks within requested startMS/endMS.\n"
        "- Ensure endMS > startMS.\n"
        "- Coverage target: produce at least " + std::to_string(minimumCoverageBlocks) + " blocks unless the requested range is extremely short.\n"
        "- Ensure every target has multiple blocks over time, not just a handful of targets.\n"
        "- Prefer simple settings and leave settings empty if unknown.\n"
        "- Do not include markdown fences.\n"
        + (extraRules.empty() ? "" : ("- " + extraRules + "\n")) +
        "Media Clip Transcript JSON:\n" + aiClipTranscriptJson +
        "\n"
        "AI Music Analysis JSON:\n" + aiMusicAnalysisJson +
        "\n"
        "Analysis JSON:\n" + analysisJson.dump() +
        "\nTargets JSON:\n" + targetJson.dump() +
        "\nOptions JSON:\n" + optionsJson.dump();
    };

    std::string prompt = buildPlanPrompt("");
    auto [response, ok] = CallLLM(prompt);
    initialPlanResponsePreview = response;
    if (!ok) {
        plan.error = response;
        return plan;
    }

    if (!parsePlanResponse(response, plan)) {
        return plan;
    }

    if (plan.blocks.size() < minimumCoverageBlocks) {
        AIMusicEffectPlan retryPlan;
        std::string retryPrompt = buildPlanPrompt(
            "Your previous output was too sparse (" + std::to_string(plan.blocks.size()) +
            " blocks). Re-plan with fuller coverage across all targets and sections.");
        auto [retryResponse, retryOk] = CallLLM(retryPrompt);
        retryPlanResponsePreview = retryResponse;
        if (retryOk && parsePlanResponse(retryResponse, retryPlan) && retryPlan.blocks.size() >= plan.blocks.size()) {
            plan.warnings.push_back("AI planner initial pass was sparse; using second-pass replan for fuller coverage.");
            plan.blocks = std::move(retryPlan.blocks);
            plan.warnings.insert(plan.warnings.end(), retryPlan.warnings.begin(), retryPlan.warnings.end());
        } else {
            plan.warnings.push_back("AI planner remained sparse after replan attempt.");
            auto trimPreview = [](const std::string& raw) {
                std::string normalized = raw;
                Replace(normalized, std::string("\n"), std::string(" "));
                Replace(normalized, std::string("\r"), std::string(" "));
                Replace(normalized, std::string("\t"), std::string(" "));
                if (normalized.size() > 220) {
                    normalized = normalized.substr(0, 220) + "...";
                }
                return normalized;
            };
            if (!initialPlanResponsePreview.empty()) {
                plan.warnings.push_back("AI first-pass raw preview: " + trimPreview(initialPlanResponsePreview));
            }
            if (!retryPlanResponsePreview.empty()) {
                plan.warnings.push_back("AI retry raw preview: " + trimPreview(retryPlanResponsePreview));
            }
        }
    }

    if (plan.blocks.empty()) {
        AIMusicEffectPlan rescuePlan;
        const size_t targetCount = std::max<size_t>(1, targets.size());
        const size_t perTargetMinimum = std::max<size_t>(2, minimumCoverageBlocks / targetCount);
        std::string rescuePrompt = buildPlanPrompt(
            "CRITICAL: Your previous output was empty. Return a NON-EMPTY blocks array. "
            "Generate at least " + std::to_string(minimumCoverageBlocks) + " total blocks and at least " +
            std::to_string(perTargetMinimum) + " blocks per target. "
            "If uncertain about settings, still emit valid blocks with empty settings and choose a safe allowed effect such as Color Wash or On.");
        auto [rescueResponse, rescueOk] = CallLLM(rescuePrompt);
        rescuePlanResponsePreview = rescueResponse;
        if (rescueOk && parsePlanResponse(rescueResponse, rescuePlan) && !rescuePlan.blocks.empty()) {
            plan.warnings.push_back("AI planner rescue pass recovered from empty output.");
            plan.blocks = std::move(rescuePlan.blocks);
            plan.warnings.insert(plan.warnings.end(), rescuePlan.warnings.begin(), rescuePlan.warnings.end());
        } else {
            auto trimPreview = [](const std::string& raw) {
                std::string normalized = raw;
                Replace(normalized, std::string("\n"), std::string(" "));
                Replace(normalized, std::string("\r"), std::string(" "));
                Replace(normalized, std::string("\t"), std::string(" "));
                if (normalized.size() > 220) {
                    normalized = normalized.substr(0, 220) + "...";
                }
                return normalized;
            };
            plan.warnings.push_back("AI planner rescue pass still returned empty output.");
            if (!rescuePlanResponsePreview.empty()) {
                plan.warnings.push_back("AI rescue raw preview: " + trimPreview(rescuePlanResponsePreview));
            }
        }
    }

    if (!plan.blocks.empty() && plan.blocks.size() < minimumCoverageBlocks) {
        std::set<std::string> coveredTargets;
        for (const auto& block : plan.blocks) {
            if (!block.targetName.empty()) {
                coveredTargets.insert(block.targetName);
            }
        }
        nlohmann::json missingTargetsJson = nlohmann::json::array();
        for (const auto& target : targets) {
            if (coveredTargets.find(target.name) == coveredTargets.end()) {
                missingTargetsJson.push_back(target.name);
            }
        }

        nlohmann::json existingBlocksPreview = nlohmann::json::array();
        const size_t maxPreviewBlocks = std::min<size_t>(plan.blocks.size(), 180);
        for (size_t i = 0; i < maxPreviewBlocks; ++i) {
            const auto& block = plan.blocks[i];
            existingBlocksPreview.push_back({
                {"targetName", block.targetName},
                {"effectName", block.effectName},
                {"startMS", block.startMS},
                {"endMS", block.endMS}
            });
        }

        const size_t deficit = minimumCoverageBlocks - plan.blocks.size();
        std::string coverageFillPrompt = buildPlanPrompt(
            "Coverage fill pass: return only additional blocks to add to the existing plan. "
            "Need at least " + std::to_string(deficit) + " extra blocks. "
            "Prioritize targets with no blocks first, then under-covered targets. "
            "Do not duplicate exact targetName+effectName+startMS+endMS combinations from ExistingBlocksPreview JSON.");
        coverageFillPrompt +=
            "\nMissingTargets JSON:\n" + missingTargetsJson.dump() +
            "\nExistingBlocksPreview JSON:\n" + existingBlocksPreview.dump();

        AIMusicEffectPlan fillPlan;
        auto [fillResponse, fillOk] = CallLLM(coverageFillPrompt);
        if (fillOk && parsePlanResponse(fillResponse, fillPlan) && !fillPlan.blocks.empty()) {
            plan.warnings.push_back("AI coverage-fill pass added " + std::to_string(fillPlan.blocks.size()) + " block(s).");
            plan.blocks.insert(plan.blocks.end(), fillPlan.blocks.begin(), fillPlan.blocks.end());
            plan.warnings.insert(plan.warnings.end(), fillPlan.warnings.begin(), fillPlan.warnings.end());
        } else {
            plan.warnings.push_back("AI coverage-fill pass did not provide additional usable blocks.");
        }
    }

    if (plan.blocks.empty()) {
        std::vector<int> cueStarts;
        cueStarts.reserve(analysis.sectionMS.size() + analysis.downbeatMS.size() + analysis.beatMS.size());
        for (int ms : analysis.sectionMS) {
            if (ms >= options.startMS && ms < options.endMS) {
                cueStarts.push_back(ms);
            }
        }
        if (cueStarts.size() < 4) {
            for (int ms : analysis.downbeatMS) {
                if (ms >= options.startMS && ms < options.endMS) {
                    cueStarts.push_back(ms);
                }
            }
        }
        if (cueStarts.size() < 8) {
            for (int ms : analysis.beatMS) {
                if (ms >= options.startMS && ms < options.endMS) {
                    cueStarts.push_back(ms);
                }
            }
        }
        if (cueStarts.empty()) {
            const int fallbackStep = 1200;
            for (int ms = options.startMS; ms < options.endMS; ms += fallbackStep) {
                cueStarts.push_back(ms);
            }
        }
        std::sort(cueStarts.begin(), cueStarts.end());
        cueStarts.erase(std::unique(cueStarts.begin(), cueStarts.end()), cueStarts.end());
        if (cueStarts.empty() || cueStarts.front() > options.startMS) {
            cueStarts.insert(cueStarts.begin(), options.startMS);
        }
        if (cueStarts.back() < options.endMS) {
            cueStarts.push_back(options.endMS);
        }

        std::vector<std::string> effects = options.allowedEffects;
        if (effects.empty()) {
            effects = {"Color Wash", "On", "Bars", "VU Meter", "Twinkle", "Spirals", "Pinwheel", "Meteors", "Butterfly", "Marquee"};
        }

        const size_t targetCount = std::max<size_t>(1, targets.size());
        size_t effectCursor = 0;
        for (size_t t = 0; t < targetCount && plan.blocks.size() < minimumCoverageBlocks; ++t) {
            const auto& target = targets[t];
            const size_t stride = 1 + (std::hash<std::string>{}(target.name) % 3);
            for (size_t i = 0; (i + 1) < cueStarts.size() && plan.blocks.size() < minimumCoverageBlocks; i += stride) {
                int startMS = cueStarts[i];
                int endMS = cueStarts[i + 1];
                if (endMS <= startMS) {
                    endMS = std::min(options.endMS, startMS + 300);
                }
                if (endMS <= startMS) {
                    continue;
                }

                AIEffectBlock block;
                block.targetName = target.name;
                block.layerHint = 0;
                block.effectName = effects[effectCursor % effects.size()];
                block.startMS = std::max(options.startMS, startMS);
                block.endMS = std::min(options.endMS, endMS);
                block.reason = "Structured fallback when AI returned empty output.";
                block.priority = static_cast<int>(plan.blocks.size());
                plan.blocks.push_back(std::move(block));
                ++effectCursor;
            }
        }

        if (!plan.blocks.empty()) {
            plan.warnings.push_back(
                "AI planner returned empty output after retries; generated structured local fallback blocks to avoid full deterministic takeover.");
        }
    }

    return plan;
}

aiBase::AIImageGenerator* OpenAIAPI::createAIImageGenerator() const {
    return new OpenAIImageGenerator(base_url, token, image_model);
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

//https://api.openai.com/v1/audio/transcriptions
aiBase::AILyricTrack OpenAIAPI::GenerateLyricTrack(const std::string& audioPath) const {
    AILyricTrack ret;
    if (token.empty()) {
        ret.error = "You must set a " + GetLLMName() + " Bearer Token in the Preferences on the Services Panel";
        return ret;
    }
    if (audioPath.empty()) {
        ret.error = "Audio path is empty.";
        return ret;
    }

    auto parseWords = [](const std::string& text) {
        std::vector<std::string> words;
        std::string current;
        for (char ch : text) {
            if (std::isspace(static_cast<unsigned char>(ch))) {
                if (!current.empty()) {
                    words.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(ch);
            }
        }
        if (!current.empty()) {
            words.push_back(current);
        }
        return words;
    };

    auto parseLyricsJson = [&](const std::string& readBuffer, AILyricTrack& out) {
        nlohmann::json root = nlohmann::json::parse(readBuffer);
        if (root.contains("error")) {
            out.error = root["error"]["message"].get<std::string>();
            return false;
        }

        if (root.contains("words") && root["words"].is_array()) {
            for (const auto& word : root["words"]) {
                if (!word.contains("word")) {
                    continue;
                }
                AILyric lyric;
                lyric.word = word["word"].get<std::string>();
                lyric.startMS = static_cast<int>(word.value("start", 0.0) * 1000.0);
                lyric.endMS = static_cast<int>(word.value("end", word.value("start", 0.0)) * 1000.0);
                if (lyric.endMS <= lyric.startMS) {
                    lyric.endMS = lyric.startMS + 250;
                }
                out.lyrics.push_back(std::move(lyric));
            }
            return !out.lyrics.empty();
        }

        if (root.contains("segments") && root["segments"].is_array()) {
            for (const auto& segment : root["segments"]) {
                if (segment.contains("words") && segment["words"].is_array()) {
                    for (const auto& word : segment["words"]) {
                        if (!word.contains("word")) {
                            continue;
                        }
                        AILyric lyric;
                        lyric.word = word["word"].get<std::string>();
                        lyric.startMS = static_cast<int>(word.value("start", 0.0) * 1000.0);
                        lyric.endMS = static_cast<int>(word.value("end", word.value("start", 0.0)) * 1000.0);
                        if (lyric.endMS <= lyric.startMS) {
                            lyric.endMS = lyric.startMS + 250;
                        }
                        out.lyrics.push_back(std::move(lyric));
                    }
                } else if (segment.contains("text")) {
                    const auto words = parseWords(segment["text"].get<std::string>());
                    if (words.empty()) {
                        continue;
                    }
                    const int segStart = static_cast<int>(segment.value("start", 0.0) * 1000.0);
                    const int segEnd = static_cast<int>(segment.value("end", segment.value("start", 0.0)) * 1000.0);
                    const int segDur = std::max(250, segEnd - segStart);
                    for (size_t wi = 0; wi < words.size(); ++wi) {
                        AILyric lyric;
                        lyric.word = words[wi];
                        lyric.startMS = segStart + static_cast<int>((segDur * wi) / words.size());
                        lyric.endMS = segStart + static_cast<int>((segDur * (wi + 1)) / words.size());
                        if (lyric.endMS <= lyric.startMS) {
                            lyric.endMS = lyric.startMS + 200;
                        }
                        out.lyrics.push_back(std::move(lyric));
                    }
                }
            }
            return !out.lyrics.empty();
        }

        if (root.contains("text") && root["text"].is_string()) {
            const auto words = parseWords(root["text"].get<std::string>());
            int t = 0;
            for (const auto& w : words) {
                AILyric lyric;
                lyric.word = w;
                lyric.startMS = t;
                lyric.endMS = t + 250;
                out.lyrics.push_back(std::move(lyric));
                t += 250;
            }
            return !out.lyrics.empty();
        }

        out.error = "Response does not contain usable transcript data.";
        return false;
    };

    auto callTranscription = [&](const char* responseFormat, bool includeWordGranularity, std::string& responseBody, std::string& errorMsg) {
        responseBody.clear();
        errorMsg.clear();
        CURL* curl = curl_easy_init();
        if (curl == nullptr) {
            errorMsg = "Failed to initialise curl.";
            return false;
        }

        curl_mime* form = curl_mime_init(curl);
        curl_mimepart* field = curl_mime_addpart(form);
        curl_mime_name(field, "file");
        curl_mime_filedata(field, audioPath.c_str());

        field = curl_mime_addpart(form);
        curl_mime_name(field, "model");
        curl_mime_data(field, transcribe_model.c_str(), CURL_ZERO_TERMINATED);

        field = curl_mime_addpart(form);
        curl_mime_name(field, "response_format");
        curl_mime_data(field, responseFormat, CURL_ZERO_TERMINATED);

        if (includeWordGranularity) {
            field = curl_mime_addpart(form);
            curl_mime_name(field, "timestamp_granularities[]");
            curl_mime_data(field, "word", CURL_ZERO_TERMINATED);
        }

        struct curl_slist* headerlist = nullptr;
        std::string authHeader = "Authorization: Bearer " + token;
        headerlist = curl_slist_append(headerlist, authHeader.c_str());

        std::string full_url = base_url + transcriptions_url;
        curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            errorMsg = "curl_easy_perform() failed: " + std::string(curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
        curl_mime_free(form);
        curl_slist_free_all(headerlist);
        return res == CURLE_OK;
    };

    curl_global_init(CURL_GLOBAL_ALL);
    std::string readBuffer;
    std::string requestError;
    bool requestOK = callTranscription("verbose_json", true, readBuffer, requestError);
    if (!requestOK) {
        ret.error = requestError;
    } else {
        spdlog::debug("OpenAI transcription response: {}", readBuffer);
        try {
            parseLyricsJson(readBuffer, ret);
            if (!ret.error.empty() && ret.error.find("response_format") != std::string::npos &&
                ret.error.find("not compatible") != std::string::npos) {
                ret.error.clear();
                ret.lyrics.clear();
                std::string fallbackBody;
                std::string fallbackError;
                if (callTranscription("json", false, fallbackBody, fallbackError)) {
                    spdlog::debug("OpenAI transcription fallback response: {}", fallbackBody);
                    parseLyricsJson(fallbackBody, ret);
                } else {
                    ret.error = fallbackError;
                }
            }
        } catch (const nlohmann::json::parse_error& e) {
            ret.error = e.what();
        } catch (const std::exception& e) {
            ret.error = e.what();
        }
    }
    curl_global_cleanup();

    return ret;
}
