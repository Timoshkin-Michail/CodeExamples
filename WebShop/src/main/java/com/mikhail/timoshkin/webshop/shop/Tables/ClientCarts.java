package com.mikhail.timoshkin.webshop.shop.Tables;

import com.mikhail.timoshkin.webshop.shop.Tables.RedisHash.Cart;
import org.springframework.data.repository.CrudRepository;
import org.springframework.stereotype.Repository;

@Repository
public interface ClientCarts extends CrudRepository<Cart, Integer> {}
