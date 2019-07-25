#ifndef RD_CPP_RDENDPOINT_H
#define RD_CPP_RDENDPOINT_H

#include "Polymorphic.h"
#include "RdTask.h"

#pragma warning( push )
#pragma warning( disable:4250 )

namespace rd {
	/**
	 * \brief An API that is exposed to the remote process and can be invoked over the protocol.
	 * 
	 * \tparam TReq type of request 
	 * \tparam TRes type of response
	 * \tparam ReqSer "SerDes" for request
	 * \tparam ResSer "SerDes" for response
	 */
	template<typename TReq, typename TRes, typename ReqSer = Polymorphic <TReq>, typename ResSer = Polymorphic <TRes> >
	class RdEndpoint : public virtual RdReactiveBase, public ISerializable {
		using WTReq = value_or_wrapper<TReq>;
		using WTRes = value_or_wrapper<TRes>;

		using handler_t = std::function<RdTask<TRes, ResSer>(Lifetime, TReq const &)>;
		mutable handler_t handler;

		mutable tsl::ordered_map<RdId, RdTask<TRes, ResSer>> awaiting_tasks;
	public:
		//region ctor/dtor

		RdEndpoint() = default;

		explicit RdEndpoint(handler_t handler) {
			set(std::move(handler));
		}

		explicit RdEndpoint(std::function<WTRes(TReq const &)

		> handler) {
			set(std::move(handler));
		}

		RdEndpoint(RdEndpoint &&) = default;

		RdEndpoint &operator=(RdEndpoint &&) = default;

		virtual ~RdEndpoint() = default;
		//endregion

		static RdEndpoint<TReq, TRes, ReqSer, ResSer> read(SerializationCtx &ctx, Buffer &buffer) {
			RdEndpoint<TReq, TRes, ReqSer, ResSer> res;
			const RdId &id = RdId::read(buffer);
			withId(res, id);
			return res;
		}

		void write(SerializationCtx &ctx, Buffer &buffer) const override {
			rdid.write(buffer);
		}

		/**
		 * \brief Assigns a handler that executes the API asynchronously.
		 * \param handler to assign
		 */
		void set(handler_t handler) const {
			RD_ASSERT_MSG(handler, "handler is set already");
			this->handler = std::move(handler);
		}

		/**
		 * \brief @see set above
		 */
		void set(std::function<WTRes(TReq const &)> handler) const {
			this->handler = [handler = std::move(handler)](Lifetime _, TReq const &req) -> RdTask <TRes, ResSer> {
				return RdTask<TRes, ResSer>::from_result(handler(req));
			};
		}

		void init(Lifetime lifetime) const override {
			RdReactiveBase::init(lifetime);
			bind_lifetime = lifetime;
			get_wire()->advise(lifetime, this);
		}

		void on_wire_received(Buffer buffer) const override {
			auto task_id = RdId::read(buffer);
			auto value = ReqSer::read(get_serialization_context(), buffer);
			logReceived.trace("endpoint %s::%s request = " + to_string(value), to_string(location).c_str(), to_string(rdid).c_str());
			if (!handler) {
				throw std::invalid_argument("handler is empty for RdEndPoint");
			}
			auto & task = awaiting_tasks[task_id] = {};
			try {
				task = handler(*bind_lifetime, wrapper::get<TReq>(value));
			} catch (std::exception const &e) {
				task.fault(e);
			}
			task.advise(*bind_lifetime, [this, task_id, &task](RdTaskResult<TRes, ResSer> const &task_result) {
				logSend.trace("endpoint %s::%s response = %s",
							  to_string(location).c_str(), to_string(rdid).c_str(), to_string(*task.result).c_str());
				get_wire()->send(task_id, [&](Buffer &inner_buffer) {
					task_result.write(get_serialization_context(), inner_buffer);
				});
				//todo remove from awaiting_tasks
			});
		}

		friend bool operator==(const RdEndpoint &lhs, const RdEndpoint &rhs) {
			return &lhs == &rhs;
		}

		friend bool operator!=(const RdEndpoint &lhs, const RdEndpoint &rhs) {
			return !(rhs == lhs);
		}

		friend std::string to_string(RdEndpoint const &value) {
			return "RdEndpoint";
		}
	};
}

#pragma warning( pop )

#endif //RD_CPP_RDENDPOINT_H
