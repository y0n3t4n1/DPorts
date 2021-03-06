--- clients/window.c.orig	2015-10-02 18:54:46 +0200
+++ clients/window.c
@@ -37,8 +37,7 @@
 #include <time.h>
 #include <cairo.h>
 #include <sys/mman.h>
-#include <sys/epoll.h>
-#include <sys/timerfd.h>
+#include <event2/event.h>
 #include <stdbool.h>
 
 #ifdef HAVE_CAIRO_EGL
@@ -106,10 +105,11 @@
 	uint32_t serial;
 
 	int display_fd;
+	struct event *display_ev;
 	uint32_t display_fd_events;
 	struct task display_task;
 
-	int epoll_fd;
+	struct event_base *evbase;
 	struct wl_list deferred_list;
 
 	int running;
@@ -321,9 +321,9 @@
 	struct wl_callback *cursor_frame_cb;
 	uint32_t cursor_timer_start;
 	uint32_t cursor_anim_current;
-	int cursor_delay_fd;
 	bool cursor_timer_running;
 	struct task cursor_task;
+	struct event *cursor_timer_ev;
 	struct wl_surface *pointer_surface;
 	uint32_t modifiers;
 	uint32_t pointer_enter_serial;
@@ -358,7 +358,7 @@
 	int32_t repeat_delay_nsec;
 
 	struct task repeat_task;
-	int repeat_timer_fd;
+	struct event *repeat_timer_ev;
 	uint32_t repeat_sym;
 	uint32_t repeat_key;
 	uint32_t repeat_time;
@@ -408,7 +408,7 @@
 	struct widget *widget;
 	char *entry;
 	struct task tooltip_task;
-	int tooltip_fd;
+	struct event *tooltipev;
 	float x, y;
 };
 
@@ -713,14 +713,14 @@
 
 	fd = os_create_anonymous_file(size);
 	if (fd < 0) {
-		fprintf(stderr, "creating a buffer file for %d B failed: %m\n",
-			size);
+		fprintf(stderr, "creating a buffer file for %d B failed: %s\n",
+			size, strerror(errno));
 		return NULL;
 	}
 
 	*data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
 	if (*data == MAP_FAILED) {
-		fprintf(stderr, "mmap failed: %m\n");
+		fprintf(stderr, "mmap failed: %s\n", strerror(errno));
 		close(fd);
 		return NULL;
 	}
@@ -2051,21 +2051,18 @@
 		tooltip->widget = NULL;
 	}
 
-	close(tooltip->tooltip_fd);
+	event_free(tooltip->tooltipev);
 	free(tooltip->entry);
 	free(tooltip);
 	parent->tooltip = NULL;
 }
 
 static void
-tooltip_func(struct task *task, uint32_t events)
+tooltip_func(evutil_socket_t fd, short what, void *arg)
 {
+	struct task *task = (struct task *)arg;
 	struct tooltip *tooltip =
 		container_of(task, struct tooltip, tooltip_task);
-	uint64_t exp;
-
-	if (read(tooltip->tooltip_fd, &exp, sizeof (uint64_t)) != sizeof (uint64_t))
-		abort();
 	window_create_tooltip(tooltip);
 }
 
@@ -2073,16 +2070,11 @@
 static int
 tooltip_timer_reset(struct tooltip *tooltip)
 {
-	struct itimerspec its;
+	struct timeval tv;
 
-	its.it_interval.tv_sec = 0;
-	its.it_interval.tv_nsec = 0;
-	its.it_value.tv_sec = TOOLTIP_TIMEOUT / 1000;
-	its.it_value.tv_nsec = (TOOLTIP_TIMEOUT % 1000) * 1000 * 1000;
-	if (timerfd_settime(tooltip->tooltip_fd, 0, &its, NULL) < 0) {
-		fprintf(stderr, "could not set timerfd\n: %m");
-		return -1;
-	}
+	tv.tv_sec = TOOLTIP_TIMEOUT / 1000;
+	tv.tv_usec = (TOOLTIP_TIMEOUT % 1000) * 1000;
+	evtimer_add(tooltip->tooltipev, &tv);
 
 	return 0;
 }
@@ -2115,15 +2107,9 @@
 	tooltip->x = x;
 	tooltip->y = y;
 	tooltip->entry = strdup(entry);
-	tooltip->tooltip_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
-	if (tooltip->tooltip_fd < 0) {
-		fprintf(stderr, "could not create timerfd\n: %m");
-		return -1;
-	}
 
-	tooltip->tooltip_task.run = tooltip_func;
-	display_watch_fd(parent->window->display, tooltip->tooltip_fd,
-			 EPOLLIN, &tooltip->tooltip_task);
+	tooltip->tooltipev = evtimer_new(parent->window->display->evbase,
+					 tooltip_func, &tooltip->tooltip_task);
 	tooltip_timer_reset(tooltip);
 
 	return 0;
@@ -2648,19 +2634,19 @@
 static void
 cursor_delay_timer_reset(struct input *input, uint32_t duration)
 {
-	struct itimerspec its;
+	struct timeval tv;
 
 	if (!duration)
 		input->cursor_timer_running = false;
 	else
 		input->cursor_timer_running = true;
 
-	its.it_interval.tv_sec = 0;
-	its.it_interval.tv_nsec = 0;
-	its.it_value.tv_sec = duration / 1000;
-	its.it_value.tv_nsec = (duration % 1000) * 1000 * 1000;
-	if (timerfd_settime(input->cursor_delay_fd, 0, &its, NULL) < 0)
-		fprintf(stderr, "could not set cursor timerfd\n: %m");
+	tv.tv_sec = duration / 1000;
+	tv.tv_usec = (duration % 1000) * 1000;
+	if (duration == 0)
+		event_del(input->repeat_timer_ev);
+	else
+		event_add(input->repeat_timer_ev, &tv);
 }
 
 static void cancel_pointer_image_update(struct input *input)
@@ -2828,13 +2814,8 @@
 input_remove_keyboard_focus(struct input *input)
 {
 	struct window *window = input->keyboard_focus;
-	struct itimerspec its;
 
-	its.it_interval.tv_sec = 0;
-	its.it_interval.tv_nsec = 0;
-	its.it_value.tv_sec = 0;
-	its.it_value.tv_nsec = 0;
-	timerfd_settime(input->repeat_timer_fd, 0, &its, NULL);
+	evtimer_del(input->repeat_timer_ev);
 
 	if (!window)
 		return;
@@ -2847,24 +2828,23 @@
 }
 
 static void
-keyboard_repeat_func(struct task *task, uint32_t events)
+keyboard_repeat_func(evutil_socket_t fd, short what, void *arg)
 {
+	struct task *task = (struct task *)arg;
 	struct input *input =
 		container_of(task, struct input, repeat_task);
 	struct window *window = input->keyboard_focus;
-	uint64_t exp;
+	struct timeval tv;
 
-	if (read(input->repeat_timer_fd, &exp, sizeof exp) != sizeof exp)
-		/* If we change the timer between the fd becoming
-		 * readable and getting here, there'll be nothing to
-		 * read and we get EAGAIN. */
-		return;
+	tv.tv_sec = input->repeat_rate_sec;
+	tv.tv_usec = input->repeat_rate_nsec / 1000;
 
 	if (window && window->key_handler) {
 		(*window->key_handler)(window, input, input->repeat_time,
 				       input->repeat_key, input->repeat_sym,
 				       WL_KEYBOARD_KEY_STATE_PRESSED,
 				       window->user_data);
+		evtimer_add(input->repeat_timer_ev, &tv);
 	}
 }
 
@@ -2963,7 +2943,7 @@
 	enum wl_keyboard_key_state state = state_w;
 	const xkb_keysym_t *syms;
 	xkb_keysym_t sym;
-	struct itimerspec its;
+	struct timeval tv;
 
 	input->display->serial = serial;
 	code = key + 8;
@@ -3003,21 +2983,21 @@
 
 	if (state == WL_KEYBOARD_KEY_STATE_RELEASED &&
 	    key == input->repeat_key) {
-		its.it_interval.tv_sec = 0;
-		its.it_interval.tv_nsec = 0;
-		its.it_value.tv_sec = 0;
-		its.it_value.tv_nsec = 0;
-		timerfd_settime(input->repeat_timer_fd, 0, &its, NULL);
+		evtimer_del(input->repeat_timer_ev);
 	} else if (state == WL_KEYBOARD_KEY_STATE_PRESSED &&
 		   xkb_keymap_key_repeats(input->xkb.keymap, code)) {
 		input->repeat_sym = sym;
 		input->repeat_key = key;
 		input->repeat_time = time;
-		its.it_interval.tv_sec = input->repeat_rate_sec;
-		its.it_interval.tv_nsec = input->repeat_rate_nsec;
-		its.it_value.tv_sec = input->repeat_delay_sec;
-		its.it_value.tv_nsec = input->repeat_delay_nsec;
-		timerfd_settime(input->repeat_timer_fd, 0, &its, NULL);
+		if (input->repeat_delay_sec == 0 &&
+		    input->repeat_delay_nsec / 1000 == 0) {
+			tv.tv_sec = input->repeat_rate_sec;
+			tv.tv_usec = input->repeat_rate_nsec / 1000;
+		} else {
+			tv.tv_sec = input->repeat_delay_sec;
+			tv.tv_usec = input->repeat_delay_nsec / 1000;
+		}
+		evtimer_add(input->repeat_timer_ev, &tv);
 	}
 }
 
@@ -3351,6 +3331,8 @@
 	data_func_t func;
 	int32_t x, y;
 	void *user_data;
+
+	struct event *ev;
 };
 
 static void
@@ -3662,20 +3644,17 @@
 }
 
 static void
-cursor_timer_func(struct task *task, uint32_t events)
+cursor_timer_func(evutil_socket_t fd, short what, void *arg)
 {
+	struct task *task = (struct task *)arg;
 	struct input *input = container_of(task, struct input, cursor_task);
 	struct timespec tp;
 	struct wl_cursor *cursor;
 	uint32_t time;
-	uint64_t exp;
 
 	if (!input->cursor_timer_running)
 		return;
 
-	if (read(input->cursor_delay_fd, &exp, sizeof (uint64_t)) != sizeof (uint64_t))
-		return;
-
 	cursor = input->display->cursors[input->current_cursor];
 	if (!cursor)
 		return;
@@ -3740,8 +3719,9 @@
 }
 
 static void
-offer_io_func(struct task *task, uint32_t events)
+offer_io_func(evutil_socket_t fd, short what, void *arg)
 {
+	struct task *task = (struct task *)arg;
 	struct data_offer *offer =
 		container_of(task, struct data_offer, io_task);
 	unsigned int len;
@@ -3763,8 +3743,15 @@
 {
 	int p[2];
 
+#if defined(__FreeBSD__)
+	if (pipe(p) == -1)
+		return;
+	fcntl(p[0], O_CLOEXEC);
+	fcntl(p[1], O_CLOEXEC);
+#else
 	if (pipe2(p, O_CLOEXEC) == -1)
 		return;
+#endif
 
 	wl_data_offer_receive(offer->offer, mime_type, p[1]);
 	close(p[1]);
@@ -3775,8 +3762,8 @@
 	offer->refcount++;
 	offer->user_data = user_data;
 
-	display_watch_fd(offer->input->display,
-			 offer->fd, EPOLLIN, &offer->io_task);
+	offer->ev = display_watch_fd(offer->input->display,
+				     offer->fd, EV_READ, &offer->io_task);
 }
 
 void
@@ -4269,8 +4256,9 @@
 }
 
 static void
-idle_redraw(struct task *task, uint32_t events)
+idle_redraw(evutil_socket_t fd, short what, void *arg)
 {
+	struct task *task = (struct task *)arg;
 	struct window *window = container_of(task, struct window, redraw_task);
 	struct surface *surface;
 	int failed = 0;
@@ -5236,19 +5224,13 @@
 	}
 
 	input->pointer_surface = wl_compositor_create_surface(d->compositor);
-	input->cursor_task.run = cursor_timer_func;
 
-	input->cursor_delay_fd = timerfd_create(CLOCK_MONOTONIC,
-						TFD_CLOEXEC | TFD_NONBLOCK);
-	display_watch_fd(d, input->cursor_delay_fd, EPOLLIN,
-			 &input->cursor_task);
+	input->cursor_timer_ev = evtimer_new(d->evbase, cursor_timer_func,
+					     &input->cursor_task);
 	set_repeat_info(input, 40, 400);
 
-	input->repeat_timer_fd = timerfd_create(CLOCK_MONOTONIC,
-						TFD_CLOEXEC | TFD_NONBLOCK);
-	input->repeat_task.run = keyboard_repeat_func;
-	display_watch_fd(d, input->repeat_timer_fd,
-			 EPOLLIN, &input->repeat_task);
+	input->repeat_timer_ev = evtimer_new(d->evbase, keyboard_repeat_func,
+					     &input->repeat_task);
 }
 
 static void
@@ -5282,8 +5264,8 @@
 
 	wl_list_remove(&input->link);
 	wl_seat_destroy(input->seat);
-	close(input->repeat_timer_fd);
-	close(input->cursor_delay_fd);
+	event_free(input->repeat_timer_ev);
+	event_free(input->cursor_timer_ev);
 	free(input);
 }
 
@@ -5522,21 +5504,14 @@
 }
 
 static void
-handle_display_data(struct task *task, uint32_t events)
+handle_display_data(evutil_socket_t fd, short what, void *arg)
 {
+	struct task *task = (struct task *)arg;
 	struct display *display =
 		container_of(task, struct display, display_task);
-	struct epoll_event ep;
 	int ret;
 
-	display->display_fd_events = events;
-
-	if (events & EPOLLERR || events & EPOLLHUP) {
-		display_exit(display);
-		return;
-	}
-
-	if (events & EPOLLIN) {
+	if (what & EV_READ) {
 		ret = wl_display_dispatch(display->display);
 		if (ret == -1) {
 			display_exit(display);
@@ -5544,13 +5519,14 @@
 		}
 	}
 
-	if (events & EPOLLOUT) {
+	if (what & EV_WRITE) {
 		ret = wl_display_flush(display->display);
 		if (ret == 0) {
-			ep.events = EPOLLIN | EPOLLERR | EPOLLHUP;
-			ep.data.ptr = &display->display_task;
-			epoll_ctl(display->epoll_fd, EPOLL_CTL_MOD,
-				  display->display_fd, &ep);
+			event_free(display->display_ev);
+			display->display_ev = event_new(display->evbase,
+			    display->display_fd, EV_PERSIST | EV_READ,
+			    handle_display_data, &display->display_task);
+			event_add(display->display_ev, NULL);
 		} else if (ret == -1 && errno != EAGAIN) {
 			display_exit(display);
 			return;
@@ -5577,7 +5553,8 @@
 
 	d->display = wl_display_connect(NULL);
 	if (d->display == NULL) {
-		fprintf(stderr, "failed to connect to Wayland display: %m\n");
+		fprintf(stderr, "failed to connect to Wayland display: %s\n",
+		    strerror(errno));
 		free(d);
 		return NULL;
 	}
@@ -5589,11 +5566,17 @@
 		return NULL;
 	}
 
-	d->epoll_fd = os_epoll_create_cloexec();
+	d->evbase = event_base_new();
 	d->display_fd = wl_display_get_fd(d->display);
+#if 0
 	d->display_task.run = handle_display_data;
 	display_watch_fd(d, d->display_fd, EPOLLIN | EPOLLERR | EPOLLHUP,
 			 &d->display_task);
+#else
+	d->display_ev = event_new(d->evbase, d->display_fd, EV_PERSIST|EV_READ,
+				  handle_display_data, &d->display_task);
+	event_add(d->display_ev, NULL);
+#endif
 
 	wl_list_init(&d->deferred_list);
 	wl_list_init(&d->input_list);
@@ -5607,7 +5590,8 @@
 	wl_registry_add_listener(d->registry, &registry_listener, d);
 
 	if (wl_display_roundtrip(d->display) < 0) {
-		fprintf(stderr, "Failed to process Wayland connection: %m\n");
+		fprintf(stderr, "Failed to process Wayland connection: %s\n",
+		    strerror(errno));
 		return NULL;
 	}
 
@@ -5692,11 +5676,9 @@
 	wl_compositor_destroy(display->compositor);
 	wl_registry_destroy(display->registry);
 
-	close(display->epoll_fd);
+	event_base_free(display->evbase);
 
-	if (!(display->display_fd_events & EPOLLERR) &&
-	    !(display->display_fd_events & EPOLLHUP))
-		wl_display_flush(display->display);
+	wl_display_flush(display->display);
 
 	wl_display_disconnect(display->display);
 	free(display);
@@ -5808,29 +5790,35 @@
 	wl_list_insert(&display->deferred_list, &task->link);
 }
 
-void
-display_watch_fd(struct display *display,
-		 int fd, uint32_t events, struct task *task)
+struct event *
+display_add_periodic_timer(struct display *display, struct task *task)
 {
-	struct epoll_event ep;
+	return event_new(display->evbase, -1, EV_PERSIST, task->run, task);
+}
 
-	ep.events = events;
-	ep.data.ptr = task;
-	epoll_ctl(display->epoll_fd, EPOLL_CTL_ADD, fd, &ep);
+struct event *
+display_add_oneshot_timer(struct display *display, struct task *task)
+{
+	return event_new(display->evbase, -1, 0, task->run, task);
 }
 
-void
-display_unwatch_fd(struct display *display, int fd)
+struct event *
+display_watch_fd(struct display *display,
+		 int fd, short what, struct task *task)
 {
-	epoll_ctl(display->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
+	struct event *ev;
+
+	ev = event_new(display->evbase, fd, what, task->run, task);
+	event_add(ev, NULL);
+
+	return ev;
 }
 
 void
 display_run(struct display *display)
 {
 	struct task *task;
-	struct epoll_event ep[16];
-	int i, count, ret;
+	int ret;
 
 	display->running = 1;
 	while (1) {
@@ -5838,7 +5826,7 @@
 			task = container_of(display->deferred_list.prev,
 					    struct task, link);
 			wl_list_remove(&task->link);
-			task->run(task, 0);
+			task->run(0, 0, task);
 		}
 
 		wl_display_dispatch_pending(display->display);
@@ -5848,22 +5836,12 @@
 
 		ret = wl_display_flush(display->display);
 		if (ret < 0 && errno == EAGAIN) {
-			ep[0].events =
-				EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP;
-			ep[0].data.ptr = &display->display_task;
-
-			epoll_ctl(display->epoll_fd, EPOLL_CTL_MOD,
-				  display->display_fd, &ep[0]);
+			/* NOTHING */
 		} else if (ret < 0) {
 			break;
 		}
 
-		count = epoll_wait(display->epoll_fd,
-				   ep, ARRAY_LENGTH(ep), -1);
-		for (i = 0; i < count; i++) {
-			task = ep[i].data.ptr;
-			task->run(task, ep[i].events);
-		}
+		event_base_loop(display->evbase, EVLOOP_ONCE);
 	}
 }
 
@@ -5922,7 +5900,7 @@
 fail_on_null(void *p)
 {
 	if (p == NULL) {
-		fprintf(stderr, "%s: out of memory\n", program_invocation_short_name);
+		fprintf(stderr, "%s: out of memory\n", getprogname());
 		exit(EXIT_FAILURE);
 	}
 
