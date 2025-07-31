#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include <boost/signals2/signal.hpp>

#include <nlohmann/json.hpp>

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
             * Creates a signal with the specified global identifier.
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
             * Gets the signal's linear-rule start and delta parameters. These values are cached
             * by set_metadata(), and so this member function is more efficient than calling
             * `metadata().linear_start_delta()`.
             *
             * @return A pair containing the signal's linear-rule start and delta parameters, in
             *     that order. If the signal does not have a linear rule, or one of the parameters
             *     is missing in the signal's metadata, zeroes are returned instead.
             */
            const std::pair<std::int64_t, std::int64_t> linear_start_delta() const noexcept;

            /**
             * Gets the metadata that describes the signal.
             *
             * @return The metadata that describes the signal.
             */
            const wss::metadata& metadata() const noexcept;

            /**
             * Gets the current sample index. This represents the total number of samples that the
             * application has published using the
             * publish_data(std::int64_t, std::size_t, const void *, std::size_t) overload, and is
             * updated each time that function is called.
             *
             * @return The current sample index.
             */
            std::size_t sample_index() const noexcept;

            /**
             * Gets the global identifier of the associated domain signal, if any. This value is
             * cached by set_metadata(), and so this member function is more efficient than
             * calling `metadata().table_id()`.
             *
             * @return The global identifier of the associated domain signal, or an empty string
             *     if no associated domain signal is specified in the signal's metadata.
             */
            const std::string& table_id() const noexcept;

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

        private:

            std::string _id;
            std::pair<std::int64_t, std::int64_t> _linear_start_delta;
            wss::metadata _metadata;
            std::string _table_id;
            std::size_t _sample_index;
    };
}
