/**
 * @file config.cpp
 * @brief Contains configuration options for the assembler.
 * @author Vishank Singh, https://github.com/VishankSingh
 */

#include "config.h"

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace vm_config
{
    VmConfig config;

    void VmConfig::modifyConfig(const std::string &section, const std::string &key, const std::string &value, bool shouldSave)
    {
        if (section == "Execution")
        {
            if (key == "processor_type")
            {
                if (value == "single_stage")
                {
                    setVmType(VmTypes::SINGLE_STAGE);
                }
                else if (value == "multi_stage")
                {
                    setVmType(VmTypes::MULTI_STAGE);
                }
                else
                {
                    throw std::invalid_argument("Unknown VM type: " + value);
                }
            }
            else if (key == "run_step_delay")
            {
                setRunStepDelay(std::stoull(value));
            }
            else if (key == "instruction_execution_limit")
            {
                setInstructionExecutionLimit(std::stoull(value));
            }
            else if (key == "hazard_detection") {
                // Do nothing for now
                setHazardDetectionEnabled(value == "true");
            } else if (key == "forwarding") {
                setForwardingEnabled(value == "true");
                // Do nothing for now
            } else if (key == "branch_prediction") {
                
                if (value == "none" || value == "always_not_taken") {
                    branch_prediction_type = BranchPredictionType::NONE;
                } else if (value == "static") {
                    branch_prediction_type = BranchPredictionType::STATIC;
                } else if (value == "dynamic_1bit") {
                    branch_prediction_type = BranchPredictionType::DYNAMIC1BIT;
                } else if (value == "dynamic_2bit") {
                    branch_prediction_type = BranchPredictionType::DYNAMIC2BIT;
                } else {
                    std::cerr << "Unknown branch prediction type: '" << value << "'. Defaulting to 'none'." << std::endl;
                    branch_prediction_type = BranchPredictionType::NONE;
                }
    
            }

            else
            {
                throw std::invalid_argument("Unknown key: " + key);
            }
        }
        else if (section == "Memory")
        {
            if (key == "memory_size")
            {
                setMemorySize(std::stoull(value, nullptr, 16));
            }
            else if (key == "memory_block_size")
            {
                setMemoryBlockSize(std::stoull(value));
            }
            else if (key == "block_size")
            {
                setMemoryBlockSize(std::stoull(value));
            }
            else if (key == "data_section_start")
            {
                setDataSectionStart(std::stoull(value, nullptr, 16));
            }
            else if (key == "text_section_start")
            {
                setTextSectionStart(std::stoull(value, nullptr, 16));
            }
            else if (key == "bss_section_start")
            {
                setBssSectionStart(std::stoull(value, nullptr, 16));
            }

            else
            {
                throw std::invalid_argument("Unknown key: " + key);
            }
        }

        else if (section == "Assembler")
        {
            if (key == "m_extension_enabled")
            {
                if (value == "true")
                {
                    setMExtensionEnabled(true);
                }
                else if (value == "false")
                {
                    setMExtensionEnabled(false);
                }
                else
                {
                    throw std::invalid_argument("Unknown value: " + value);
                }
            }
            else if (key == "f_extension_enabled")
            {
                if (value == "true")
                {
                    setFExtensionEnabled(true);
                }
                else if (value == "false")
                {
                    setFExtensionEnabled(false);
                }
                else
                {
                    throw std::invalid_argument("Unknown value: " + value);
                }
            }
            else if (key == "d_extension_enabled")
            {
                if (value == "true")
                {
                    setDExtensionEnabled(true);
                }
                else if (value == "false")
                {
                    setDExtensionEnabled(false);
                }
                else
                {
                    throw std::invalid_argument("Unknown value: " + value);
                }
            }
        }

        else if (section == "General") {
            // Do nothing for now (e.g., for 'name=vm')
        } else if (section == "Cache") {
            // Do nothing for now
        } else if (section == "BranchPrediction") {
            if (key == "branch_prediction_type") {
                if (value == "none" || value == "always_not_taken") {
                    setBranchPredictionType(BranchPredictionType::NONE);
                } else if (value == "static") {
                    setBranchPredictionType(BranchPredictionType::STATIC);
                } else if (value == "dynamic_1bit") {
                    setBranchPredictionType(BranchPredictionType::DYNAMIC1BIT);
                } else if (value == "dynamic_2bit") {
                    setBranchPredictionType(BranchPredictionType::DYNAMIC2BIT);
                } else {
                    std::cerr << "Unknown branch prediction type: '" << value << "'. Defaulting to 'none'." << std::endl;
                    setBranchPredictionType(BranchPredictionType::NONE);
                }
            }
        }
        else
        {
            throw std::invalid_argument("Unknown section: " + section);
        }

        if (shouldSave) {

            try
            {
                saveConfig(globals::config_file_path);
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error: Failed to save configuration: " << e.what() << std::endl;
            }

        }
        
    }

    void VmConfig::loadConfig(const std::filesystem::path &config_path)
    {

        std::ifstream config_file(config_path);
        if (!config_file.is_open())
        {
            throw std::runtime_error("Failed to open config file: " + config_path.string());
        }

        std::string line;
        std::string currentSelection;

        int lineNum = 0;

        while (std::getline(config_file, line))
        {

            lineNum++;

            line.erase(0, line.find_first_not_of(" \t\n\r"));
            line.erase(line.find_last_not_of(" \t\n\r") + 1);

            if (line.empty() || line[0] == ';' || line[0] == '#')
                continue;

            if (line[0] == '[' && line.back() == ']')
            {
                currentSelection = line.substr(1, line.length() - 2);
                continue;
            }

            size_t equalsPosition = line.find('=');

            if (equalsPosition != std::string::npos)
            {

                std::string key = line.substr(0, equalsPosition);
                std::string value = line.substr(equalsPosition + 1);

                key.erase(key.find_last_not_of(" \t\n\r") + 1);
                value.erase(0, value.find_first_not_of(" \t\n\r"));

                size_t commentPosition = value.find_first_of(";#");
                if (commentPosition != std::string::npos)
                {
                    value.erase(commentPosition);
                    value.erase(value.find_last_not_of(" \t\n\r") + 1);
                }

                try
                {
                    modifyConfig(currentSelection, key, value, false);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Warning: Error parsing config file line " << lineNum << ": '" << line << "' - " << e.what() << std::endl;
                }
            }
            else
            {
                std::cerr << "Warning: Invalid line in config file (line " << lineNum << "): '" << line << "'" << std::endl;
            }
        }

        config_file.close();
        std::cout << "Configuration loaded from: " << config_path << std::endl;
    }

    void VmConfig::saveConfig(const std::filesystem::path &config_path) const {

        std::ofstream config_file(config_path);
        if (!config_file.is_open()) {
            throw std::runtime_error("Unable to open config file for saving: " + config_path.string());
        }

        config_file << "[General]\n";
        config_file << "name=vm\n\n";

        config_file << "[Execution]\n";
        config_file << "run_step_delay=" << getRunStepDelay() << "   ; in ms\n";
        config_file << "processor_type=" << getVmTypeString() << "\n";
        config_file << "hazard_detection=" << (isHazardDetectionEnabled() ? "true" : "false") << "\n";
        config_file << "forwarding=" << (isForwardingEnabled() ? "true" : "false") << "\n";
        config_file << "branch_prediction=" << getBranchPredictionTypeString() << "\n";
        config_file << "instruction_execution_limit=" << instruction_execution_limit << "\n\n";

        config_file << "[Memory]\n";
        config_file << "memory_size=0x" << std::hex << getMemorySize() << std::dec << "\n";
        config_file << "block_size=" << getMemoryBlockSize() << "\n";
        config_file << "data_section_start=0x" << std::hex << getDataSectionStart() << std::dec << "\n";
        config_file << "text_section_start=0x" << std::hex << getTextSectionStart() << std::dec << "\n";
        config_file << "bss_section_start=0x" << std::hex << getBssSectionStart() << std::dec << "\n\n";

        config_file << "[Assembler]\n";
        config_file << "m_extension_enabled=" << (getMExtensionEnabled() ? "true" : "false") << "\n";
        config_file << "f_extension_enabled=" << (getFExtensionEnabled() ? "true" : "false") << "\n";
        config_file << "d_extension_enabled=" << (getDExtensionEnabled() ? "true" : "false") << "\n\n";

        config_file << "[Cache]\n";
        config_file << "cache_enabled=false\n";
        config_file << "cache_size=0\n";
        config_file << "cache_block_size=0\n";
        config_file << "cache_associativity=0\n";
        config_file << "cache_read_miss_policy=read_allocate\n";
        config_file << "cache_replacement_policy=LRU\n";
        config_file << "cache_write_hit_policy=write_back\n";
        config_file << "cache_write_miss_policy=write_allocate\n\n";
        
        config_file.close();

    }

}
