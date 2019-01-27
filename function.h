#ifndef function_H
#define function_H
#define _ENABLE_EXTENDED_ALIGNED_STORAGE

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <type_traits>

namespace exam {
	using std::forward;
	using std::move;
	using std::unique_ptr;
	using std::make_unique;

	constexpr size_t MAX_SIZE = 64;
	constexpr size_t MAX_ALIGN = 64;

	template <typename T>
	class function;

	template <typename R, typename... Args>
	class function<R(Args...)> {
		typedef std::aligned_storage<MAX_SIZE, MAX_ALIGN>::type small_obj;

	public:
		function() noexcept : small(false), big_object() {}

		function(std::nullptr_t) noexcept : function() {}

		template<typename F>
		function(F other) {
			if constexpr (sizeof(F) <= MAX_SIZE && alignof(F) <= MAX_ALIGN && std::is_nothrow_constructible<F>::value) {
				small = true;
				new (&small_object) func_holder<F>(move(other));
			}
			else {
				small = false;
				big_object = make_unique<func_holder<F>>(move(other));
			}
		}

		function(function const& other) : small(other.small) {
			if (small) {
				copy_small_object(&other.small_object, &small_object);
			}
			else if (other.big_object) {
				big_object = other.big_object->copy();
			}
		}

		function(function&& other) noexcept : small(other.small) {
			if (small) {
				move_small_object(&other.small_object, &small_object);
			}
			else {
				big_object = move(other.big_object);
			}
		}

		void swap(function& other) noexcept {
			using std::swap;

			if (other.small) {
				small_obj tmp;
				move_small_object(&other.small_object, &tmp);

				if (small) {
					move_small_object(&small_object, &other.small_object);
				}
				else {
					swap(big_object, other.big_object);
				}

				move_small_object(&tmp, &small_object);
			}
			else {
				if (small) {
					auto tmp = move(other.big_object);
					move_small_object(&small_object, &other.small_object);
					big_object = move(tmp);
				}
				else {
					swap(big_object, other.big_object);
				}
			}
			swap(small, other.small);
		}

		R operator()(Args&&...args) const {
			if (small) {
				return reinterpret_cast<func_holder_base*>(&small_object)->call(forward<Args>(args)...);
			}
			else if (big_object) {
				return big_object->call(forward<Args>(args)...);
			}
			else {
				throw std::bad_function_call();
			}
		}

		function& operator=(function&& other) noexcept {
			auto tmp(move(other));
			swap(tmp);
			return *this;
		}

		function& operator=(function const& other) {
			auto tmp(other);
			swap(tmp);
			return *this;
		}

		explicit operator bool() const noexcept {
			return big_object || small;
		}

		~function() {
			if (small) {
				reinterpret_cast<func_holder_base*>(&small_object)->~func_holder_base();
			}
			else {
				big_object.reset();
			}
		}

	private:
		class func_holder_base {
		public:
			func_holder_base() noexcept {}
			virtual ~func_holder_base() noexcept {}
			virtual R call(Args&&... args) = 0;
			virtual unique_ptr<func_holder_base> copy() const = 0;
			virtual void move_to(void* to) noexcept = 0;
			virtual void copy_to(void* to) const = 0;

			func_holder_base(func_holder_base const&) = delete;
			void operator=(func_holder_base const&) = delete;
		};

		template<typename F>
		class func_holder : public func_holder_base {
			F func;
		public:
			func_holder() noexcept : func_holder_base() {}
			func_holder(F const& other) noexcept : func_holder_base(), func(other) {}
			func_holder(F&& other) noexcept : func(move(other)) {}

			R call(Args&&... args) {
				return func(forward<Args>(args)...);
			}

			unique_ptr<func_holder_base> copy() const {
				return make_unique<func_holder>(func);
			}

			void move_to(void* to) noexcept {
				new (to) func_holder<F>(move(func));
			}

			void copy_to(void* to) const {
				new (to) func_holder<F>(func);
			}
		};

		void move_small_object(void* from, void* to) noexcept {
			reinterpret_cast<func_holder_base*>(from)->move_to(to);
		}

		void copy_small_object(void* from, void* to) const {
			reinterpret_cast<func_holder_base const*>(from)->copy_to(to);
		}

		bool small;
		union {
			mutable small_obj small_object;
			unique_ptr<func_holder_base> big_object;
		};
	};
}

#endif //function_H