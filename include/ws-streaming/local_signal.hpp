#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

#include <boost/signals2/signal.hpp>

#include <ws-streaming/metadata.hpp>

namespace wss
{
    /**
     * Represents a signal, sourced by the application, which can be streamed. This class
     * encapsulates the interactions between an application that generates data to be streamed and
     * the WebSocket Streaming implementation. Applications wishing to stream outgoing data should
     * create a local_signal instance for each signal to be streamed, and register that instance
     * with streaming endpoints such as wss::server.
     *
     * Applications always own their local_signal instances, and must ensure the objects are not
     * destroyed while still registered with a streaming endpoint.
     *
     * This class is not thread-safe. Applications must not call any member functions
     * concurrently. However, calls need not be synchronized with streaming endpoints. In
     * particular, it is safe to call publish_data() from an acquisition loop thread, so
     * long as no other application threads concurrently call local_signal member functions.
     */
    class local_signal
    {
        public:

            /**
             * Constructs a signal with the specified global identifier.
             *
             * @param id The signal's global identifier. This string must be unique among all
             *     signals registered with a particular streaming endpoint. The behavior is
             *     undefined if two signals with the same identifier are registered with a single
             *     endpoint.
             * @param metadata Metadata describing the signal. The metadata_builder class can be
             *     used to build signal metadata.
             */
            local_signal(const std::string& id, const metadata& metadata);

            /**
             * Sets or updates the metadata describing the signal. The metadata_builder class can
             * be used to build signal metadata. If this signal is registered with an endpoint,
             * and a remote streaming peer is subscribed to the signal, updated metadata is
             * transmitted to that peer.
             *
             * @param metadata The new metadata describing the signal.
             */
            void set_metadata(const metadata& metadata);

            /**
             * Publishes data to the signal. If this signal is registered with an endpoint,
             * and a remote streaming peer is subscribed to the signal, the data is transmitted to
             * that peer.
             *
             * For publishing value data with an associated linear-rule domain signal, use the
             * publish_data(std::int64_t, std::size_t, const void *, std::size_t) overload instead,
             * which takes additional parameters for the domain value and number of samples.
             *
             * @param data A pointer to the data to publish. The data is transmitted or (if
             *     necessary) buffered synchronously. The pointed-to memory does not need to
             *     remain valid after this function returns: the application can reuse or free it
             *     immediately.
             * @param size The number of bytes pointed to by @p data.
             */
            void publish_data(
                const void *data,
                std::size_t size)
                noexcept;

            /**
             * Publishes data to the signal with an associated linear-rule domain signal. If this
             * signal is registered with an endpoint, and a remote streaming peer is subscribed to
             * the signal, the data is transmitted to that peer.
             *
             * This overload should only be used for signals with associated linear-rule domain
             * signals. It performs the necessary bookkeeping for a linear-rule domain, and
             * transmits domain signal data packets when needed.
             *
             * @param domain_value The domain value associated with this data.
             * @param sample_count The number of samples contained in the data pointed to by
             *     @p data. This value is used to update the internal counters used to implement
             *     implicit linear-rule domains.
             * @param data A pointer to the data to publish. The data is transmitted or (if
             *     necessary) buffered synchronously. The pointed-to memory does not need to
             *     remain valid after this function returns: the application can reuse or free it
             *     immediately.
             * @param size The number of bytes pointed to by @p data.
             */
            void publish_data(
                std::int64_t domain_value,
                std::size_t sample_count,
                const void *data,
                std::size_t size)
                noexcept;

            /**
             * Gets the signal's global identifier.
             *
             * @return The signal's global identifier.
             */
            const std::string& id() const noexcept;

            /**
             * Gets the metadata that describes the signal.
             *
             * @return The metadata that describes the signal.
             */
            const wss::metadata& metadata() const noexcept;

            /**
             * Tests whether one or more remote peers are subscribed to this signal.
             *
             * @return True if one or more remote peers are subscribed to this signal.
             */
            bool is_subscribed() const noexcept;

            /**
             * An event raised when a remote peer has subscribed to the signal. This event can be
             * used to signal the application to begin publishing data for the signal. Use of this
             * event is optional; applications may choose to publish data for all signals
             * regardless of whether a peer is subscribed.
             */
            boost::signals2::signal<void()> on_subscribed;

            /**
             * An event raised when all remote peers have unsubscribed from the signal. This event
             * can be used to signal the application to stop publishing data for the signal. Use
             * of this event is optional; applications may choose to publish data for all signals
             * regardless of whether a peer is subscribed.
             */
            boost::signals2::signal<void()> on_unsubscribed;

            /**
             * An event raised when the signal's metadata is changed as a result of the
             * application calling set_metadata(). This event is used internally by streaming
             * endpoints with which this signal is registered.
             *
             * @throws ... The behavior is undefined if an attached event handler throws an
             *     exception.
             */
            boost::signals2::signal<void()> on_metadata_changed;

            /**
             * An event raised when the application publishes signal data by calling
             * publish_data(). This event is used internally by streaming endpoints with which
             * this signal is registered.
             *
             * @param domain_value The domain value passed to publish_data(), or zero if the
             *     publish_data() overload without domain information was called.
             * @param sample_count The sample count passed to publish_data(), or zero if the
             *     publish_data() overload without domain information was called.
             * @param data A pointer to the published data.
             * @param size The number of bytes pointed to by @p data.
             *
             * @throws ... The behavior is undefined if an attached event handler throws an
             *     exception.
             */
            boost::signals2::signal<
                void(
                    std::int64_t domain_value,
                    std::size_t sample_count,
                    const void *data,
                    std::size_t size)
            > on_data_published;

            /**
             * An RAII object which a caller can hold while a remote peer is subscribed to a
             * local_signal. Instances of this object are returned by increment_subscribe_count().
             * When destroyed, the subscribe count of the local_signal is correspondingly
             * decremented, possibly triggering the on_unsubscribed event if the count reaches
             * zero.
             */
            class subscribe_holder
            {
                public:

                    /**
                     * Constructs an RAII object which does not track a local signal.
                     */
                    subscribe_holder() : _signal(nullptr) { }

                    /**
                     * Constructs an RAII object tracking a local signal, incrementing its
                     * subscription reference count.
                     *
                     * @param signal The local signal to track.
                     */
                    subscribe_holder(local_signal& signal)
                        : _signal(&signal)
                    {
                        if (_signal->_subscribe_count++ == 0)
                            _signal->on_subscribed();
                    }

                    subscribe_holder(const subscribe_holder&) = delete;
                    subscribe_holder& operator=(const subscribe_holder&) = delete;

                    /**
                     * Constructs an RAII object which takes over tracking of the local signal
                     * tracked by another RAII object.
                     *
                     * @param rhs Another RAII object. After the call, @p rhs does not track a
                     *     signal.
                     */
                    subscribe_holder(subscribe_holder&& rhs) : _signal(nullptr)
                    {
                        std::swap(_signal, rhs._signal);
                    }

                    /**
                     * Takes over tracking of the local signal tracked by another RAII object. If
                     * this instance previously tracked a local signal, that signal's subscription
                     * reference count is decremented.
                     *
                     * @param rhs Another RAII object. After the call, @p rhs does not track a
                     *     signal.
                     *
                     * @return A reference to this object.
                     */
                    subscribe_holder& operator=(subscribe_holder&& rhs)
                    {
                        close();
                        std::swap(_signal, rhs._signal);
                        return *this;
                    }

                    /**
                     * Stops tracking a signal, as if by calling close().
                     */
                    ~subscribe_holder()
                    {
                        close();
                    }

                    /**
                     * Stops tracking the tracked signal (if any), decrementing its subscription
                     * reference count and possibly triggering its on_unsubscribed event.
                     */
                    void close()
                    {
                        if (_signal)
                        {
                            if (--_signal->_subscribe_count == 0)
                                _signal->on_unsubscribed();
                            _signal = nullptr;
                        }
                    }

                private:

                    local_signal *_signal;
            };

            /**
             * Increments this signal's subscription reference count. This function is used
             * internally by the library, and is called when another object is interested in
             * consuming data from this signal. This function may trigger the on_subscribed event,
             * which can be used by applications to implement lazy data publishing.
             *
             * @return An RAII object which, when destroyed, correspondingly decrements the
             *     subscription reference count, possibly triggering this signal's on_unsubscribed
             *     event.
             */
            subscribe_holder increment_subscribe_count();

        private:

            std::string             _id;
            wss::metadata           _metadata;
            std::atomic<unsigned>   _subscribe_count    = 0;
    };
}
