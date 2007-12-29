/*
 * Copyright (C) 2003 Robert Kooima
 *
 * NEVERBALL is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 2  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */

#include "gui.h"
#include "vec3.h"
#include "demo.h"
#include "game.h"
#include "audio.h"
#include "config.h"
#include "st_shared.h"

#include "st_title.h"
#include "st_help.h"
#include "st_demo.h"
#include "st_conf.h"
#include "st_set.h"
#include "st_name.h"

/*---------------------------------------------------------------------------*/

static float real_time = 0.0f;
static float demo_time = 0.0f;
static int   mode      = 0;

static int play_id = 0;

#define TITLE_PLAY 1
#define TITLE_HELP 2
#define TITLE_DEMO 3
#define TITLE_CONF 4
#define TITLE_EXIT 5

static int title_action(int i)
{
    static const char keyphrase[] = "CHEAT";
    static char queue[sizeof (keyphrase)] = "";

    size_t queue_len = strlen(queue);

    char player[MAXNAM];

    audio_play(AUD_MENU, 1.0f);

    switch (i)
    {
    case TITLE_PLAY:
        config_get_s(CONFIG_PLAYER, player, MAXNAM);

        if (strlen(player) == 0)
            return goto_name(&st_set, &st_title, 0);
        else
            return goto_state(&st_set);

        break;

    case TITLE_HELP: return goto_state(&st_help); break;
    case TITLE_DEMO: return goto_state(&st_demo); break;
    case TITLE_CONF: return goto_state(&st_conf); break;
    case TITLE_EXIT: return 0;                    break;

    default:

        /* Let the queue fill up. */

        if (queue_len < sizeof (queue) - 1)
        {
            queue[queue_len]     = (char) i;
            queue[queue_len + 1] = '\0';
        }

        /* Advance the queue before adding the new element. */

        else
        {
            int k;

            for (k = 1; k < queue_len; k++)
                queue[k - 1] = queue[k];

            queue[queue_len - 1] = (char) i;
        }

        if (strcmp(queue, keyphrase) == 0)
        {
            config_set_cheat();
            gui_set_label(play_id, sgettext("menu^Cheat"));
            gui_pulse(play_id, 1.2f);
        }
        else if (config_cheat())
        {
            config_clr_cheat();
            gui_set_label(play_id, sgettext("menu^Play"));
            gui_pulse(play_id, 1.2f);
        }

        break;
    }
    return 1;
}

static struct level title_level;

static int title_enter(void)
{
    int id, jd, kd;

    /* Build the title GUI. */

    if ((id = gui_vstack(0)))
    {
        gui_label(id, "Neverball", GUI_LRG, GUI_ALL, 0, 0);

        gui_space(id);

        if ((jd = gui_harray(id)))
        {
            gui_filler(jd);

            if ((kd = gui_varray(jd)))
            {
                if (config_cheat())
                    play_id = gui_start(kd, sgettext("menu^Cheat"),
                                        GUI_MED, TITLE_PLAY, 1);
                else
                    play_id = gui_start(kd, sgettext("menu^Play"),
                                        GUI_MED, TITLE_PLAY, 1);

                gui_state(kd, sgettext("menu^Replay"),  GUI_MED, TITLE_DEMO, 0);
                gui_state(kd, sgettext("menu^Help"),    GUI_MED, TITLE_HELP, 0);
                gui_state(kd, sgettext("menu^Options"), GUI_MED, TITLE_CONF, 0);
                gui_state(kd, sgettext("menu^Exit"),    GUI_MED, TITLE_EXIT, 0);
            }

            gui_filler(jd);
        }
        gui_layout(id, 0, 0);
    }

    /* Start the title screen music. */

    audio_music_fade_to(0.5f, "bgm/title.ogg");

    /* Initialize the title level for display. */
    level_load("map-medium/title.sol", &title_level);
    game_init(&title_level, 0, 0);

    real_time = 0.0f;
    demo_time = 0.0f;
    mode = 0;

    SDL_EnableUNICODE(1);

    return id;
}

static void title_leave(int id)
{
    SDL_EnableUNICODE(0);
    demo_replay_stop(0);
    gui_delete(id);
}

static void title_timer(int id, float dt)
{
    static const char *demo = NULL;
    float t;

    real_time += dt;

    switch (mode)
    {
    case 0: /* Mode 0: Pan across title level. */

        if (real_time <= 20.0f)
            game_set_fly(fcosf(V_PI * real_time / 20.0f));
        else
        {
            game_fade(+1.0f);
            real_time = 0.0f;
            mode = 1;
        }
        break;

    case 1: /* Mode 1: Fade out.  Load demo level. */

        if (real_time > 1.0f)
        {
            if ((demo = demo_pick()))
            {
                demo_replay_init(demo, NULL);
                demo_time = 0.0f;
                real_time = 0.0f;
                mode = 2;
            }
            else
            {
                game_fade(-1.0f);
                real_time = 0.0f;
                mode = 0;
            }
        }
        break;

    case 2: /* Mode 2: Run demo. */

        while (demo_time < real_time)
            if (demo_replay_step(&t))
                demo_time += t;
            else
            {
                demo_replay_stop(0);
                game_fade(+1.0f);
                real_time = 0.0f;
                mode = 3;
            }
        break;

    case 3: /* Mode 3: Fade out.  Load title level. */

        if (real_time > 1.0f)
        {
            game_init(&title_level, 0, 0);
            real_time = 0.0f;
            mode = 0;
        }
        break;
    }

    gui_timer(id, dt);
    game_step_fade(dt);
}

static int title_keybd(int c, int d)
{
    if (d && (c & 0xFF80) == 0 && c > ' ')
        return title_action(c);
    return 1;
}

static int title_buttn(int b, int d)
{
    if (d)
    {
        if (config_tst_d(CONFIG_JOYSTICK_BUTTON_A, b))
            return title_action(gui_token(gui_click()));
        if (config_tst_d(CONFIG_JOYSTICK_BUTTON_EXIT, b))
            return 0;
    }
    return 1;
}

/*---------------------------------------------------------------------------*/

struct state st_title = {
    title_enter,
    title_leave,
    shared_paint,
    title_timer,
    shared_point,
    shared_stick,
    shared_click,
    title_keybd,
    title_buttn,
    1, 0
};

