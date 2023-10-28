#include "editor.h"

SongExport::SongExport(SongEditor& editor, const char* file_name, int sample_rate)
:   editor(editor),
    _error(),
    is_done(false),
    total_frames(0),
    _step(0),
    modctx(sample_rate, 2, 64)
{
    // calculate length of song
    std::unique_ptr<Song>& orig_song = editor.song;

    int beat_len = orig_song->length() * orig_song->beats_per_bar;
    float sec_len = (float)beat_len * (60.0f / orig_song->tempo);
    total_frames = sec_len * sample_rate;

    // create destination node
    // create a clone of the song
    std::stringstream song_serialized;
    orig_song->serialize(song_serialized);
    song = std::unique_ptr<Song>(Song::from_file(
        song_serialized,
        modctx, 
        editor.plugin_manager,
        nullptr
    ));

    if (song == nullptr) {
        _error = "Error while exporting song";
        return;
    }

    // open file
    out_file.open(file_name, std::ios::out | std::ios::trunc | std::ios::binary);

    // if could not open file?
    if (!out_file.is_open()) {
        _error = std::string("Could not save to ") + file_name;
        return;
    }

    // create writer
    writer = std::make_unique<audiofile::WavWriter>(
        out_file,
        total_frames,
        modctx.num_channels,
        modctx.sample_rate
    );

    // set up song for exporting
    is_done = false;

    song->bar_position = 0;
    song->is_playing = true;
    song->do_loop = false;
    song->play();
    //destination.prepare();
}

void SongExport::process()
{
    if (is_done) return;

    while (song->is_playing) {
        song->update(1.0 / modctx.sample_rate * modctx.frames_per_buffer);

        float* buf;
        size_t buf_size = modctx.process(buf);
        song->work_scheduler.run();

        writer->write_block(buf, buf_size);

        _step++;

        if (_step > 1000) {
            _step = 0;
            return;
        }
    }

    if (!is_done)
    {
        is_done = true;
        out_file.close();
        song = nullptr;
        //audiomod::ModuleBase::free_garbage_modules();
    }
}

float SongExport::get_progress() const
{
    return (float)writer->written_samples / writer->total_samples;
}