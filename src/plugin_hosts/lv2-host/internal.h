#pragma once
#include <lilv/lilv.h>
#include <lv2/log/log.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/worker/worker.h>
#include <vector>
#include <string>
#include "../../worker.h"

#define RDFS_PREFIX "http://www.w3.org/2000/01/rdf-schema#"

namespace lv2 {
    extern LilvWorld* LILV_WORLD;

    struct LilvNodeUriList {
        LilvNode* a;
        LilvNode* rdfs_label;
        LilvNode* rdfs_range;

        LilvNode* lv2_InputPort;
        LilvNode* lv2_OutputPort;
        LilvNode* lv2_AudioPort;
        LilvNode* lv2_ControlPort;
        LilvNode* lv2_designation;
        LilvNode* lv2_control;
        LilvNode* lv2_connectionOptional;
        LilvNode* lv2_Parameter;

        LilvNode* atom_AtomPort;
        LilvNode* atom_bufferType;
        LilvNode* atom_Double;
        LilvNode* atom_Sequence;
        LilvNode* atom_supports;

        LilvNode* midi_MidiEvent;

        LilvNode* time_Position;

        LilvNode* patch_Message;
        LilvNode* patch_writable;
        LilvNode* patch_readable;

        LilvNode* state_interface;
        LilvNode* state_loadDefaultState;

        LilvNode* units_unit;
        LilvNode* units_db;
    } extern URI;

    /**
    * A wrapper around a LilvNode* that is
    * automatically freed when it goes out of scope
    **/
    struct LilvNode_ptr
    {
    private:
        LilvNode* node;
    public:
        LilvNode_ptr() : node(nullptr) {};
        LilvNode_ptr(LilvNode* node) : node(node) {};
        ~LilvNode_ptr() {
            if (node)
                lilv_node_free(node);
        }

        inline LilvNode* operator->() const noexcept {
            return node;
        }

        inline operator LilvNode*() const noexcept {
            return node;
        }

        inline operator bool() const noexcept {
            return node;
        }

        inline LilvNode* operator=(LilvNode* new_node) noexcept {
            node = new_node;
            return node;
        }
    };

    namespace uri {
        static std::vector<std::string> URI_MAP;

        LV2_URID map(const char* uri);
        const char* unmap(LV2_URID urid);

        // support for lv2 urid feature
        LV2_URID map_callback(LV2_URID_Map_Handle handle, const char* uri);
        const char* unmap_callback(LV2_URID_Map_Handle handle, LV2_URID urid);
    }

    namespace log {
        int printf(LV2_Log_Handle handle, LV2_URID type, const char* fmt, ...);
        int vprintf(LV2_Log_Handle handle, LV2_URID type, const char* fmt, va_list ap);
    }

    class WorkerHost
    {
    private:
        // a payload will contain a pointer to this class at the start
        static constexpr size_t USERDATA_CAPACITY = WorkScheduler::DATA_CAPACITY - sizeof(WorkerHost*);
        static constexpr size_t RESPONSE_QUEUE_CAPACITY = WorkScheduler::QUEUE_CAPACITY * 4;
        
        struct work_data_payload_t
        {
            WorkerHost* self;
            uint8_t userdata[USERDATA_CAPACITY];
        };

        struct WorkerResponse {
            std::atomic<bool> active = false;
            size_t size;
            uint8_t data[USERDATA_CAPACITY];
        } worker_responses[RESPONSE_QUEUE_CAPACITY];

    
    public:
        WorkerHost(WorkScheduler& work_scheduler);

        WorkScheduler& work_scheduler;
        LV2_Worker_Interface* worker_interface;
        LV2_Handle instance;

        static LV2_Worker_Status _schedule_work(
            LV2_Worker_Schedule_Handle handle,
            uint32_t size, const void* data
        );

        static void _work_proc(void* data, size_t size);
        static LV2_Worker_Status _worker_respond(
            LV2_Worker_Respond_Handle handle,
            uint32_t size,
            const void* data
        );

        void process_responses();
        void end_run();
    };
} // namespace lv2